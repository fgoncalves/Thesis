#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include "nack.h"
#include "sockiface.h"
#include "reliable_transport.h"
#include "connection_table.h"
#include "recv_buff.h"
#include "configuration_user_communication.h"

uint32_t window;
uint32_t recvsize;
uint32_t sack_timeout; //seconds

typedef struct {
  pthread_t deliver;
  connection_table* ct;
  pthread_mutex_t app_lock;
} control_data;

static int8_t schedule_message(__tp(handler)* sk, char* data, uint16_t len,
    uint16_t dst_app) {
  static int32_t seq = 0;
  struct timespec ts;
  msg* m = alloc(msg, 1);

  m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)) + len);
  m->dst_app_id = dst_app;
  m->pdu->id = sk->id;
  m->pdu->len = len;
  m->pdu->seq = seq++;
  clock_gettime(CLOCK_REALTIME, &ts);
  m->pdu->timestamp = timespec_to_ns(ts);
  memcpy(m->pdu->data, data, len);

  debug(I, "Scheduled message %u to be sent. It has %uB\n", m->pdu->seq, m->pdu->len);
  __tp(produce)(sk->outgoing_messages, m);

  return 1;
}

static int8_t schedule_message_with_ts(__tp(handler)* sk, char* data,
    uint16_t len, uint16_t dst_app, int64_t timestamp) {
  static int32_t seq = 0;
  struct timespec ts;
  msg* m = alloc(msg, 1);

  m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)) + len);
  m->dst_app_id = dst_app;
  m->pdu->id = sk->id;
  m->pdu->len = len;
  m->pdu->seq = seq++;
  m->pdu->timestamp = timestamp;
  m->pdu->flags.app_synch = 1;
  memcpy(m->pdu->data, data, len);

  debug(I, "Scheduled message %u to be sent. It has %uB\n", m->pdu->seq, m->pdu->len);
  __tp(produce)(sk->outgoing_messages, m);

  return 1;
}

static int8_t stop_reliable_transport(__tp(handler)* sk) {
  control_data* cd = (control_data*) sk->transport.control_data;
  tree_iterator* it = new_tree_iterator(cd->ct);
  while (tree_iterator_has_next(it)) {
    msg* m = alloc(msg, 1);

    rt_connection *c = (rt_connection*) tree_iterator_next(it);

    m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
    m->dst_app_id = c->buff->hid;
    m->pdu->id = sk->id;
    m->pdu->flags.con_terminate = 1;

    debug(I, "Sending terminate to: %d\n", c->buff->hid);

    __tp(produce)(sk->outgoing_messages, m);
  }
  destroy_iterator(it);
  pthread_cancel(cd->deliver);
  pthread_mutex_lock(&(cd->app_lock));
  free_connection_table(cd->ct);
  pthread_mutex_destroy(&(cd->app_lock));
  free(cd);
  return 1;
}

static void handle_con_terminate(__tp(handler)* sk, uint16_t hid) {
  control_data* cd = (control_data*) sk->transport.control_data;
  debug(I, "Terminating connection with %d\n", hid);
  rt_connection* rt = get_connection(cd->ct, hid, window);
  remove_connection(cd->ct, hid);
  free_rtconnection(rt);
  pthread_mutex_unlock(&(cd->app_lock));
  return;
}

static void send_oseq(__tp(handler)* sk, uint16_t dst_hid, uint32_t seq) {
  __tp(pdu)* pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)) + sizeof(seq));
  pdu->flags.oseq = 1;
  pdu->id = sk->id;
  pdu->len = sizeof(seq);
  seq = htonl(seq);
  memcpy(pdu->data, &seq, sizeof(seq));

  int status;
  struct sockaddr dst_addr;
  if ((status = get_handler_address(dst_hid, &dst_addr,
      sk->module_communication.regd)) < 0) {
    log(E, "Unable to get destination address for app %d. Error: %s\n",
        dst_hid, registry_error_str(-status));
  } else {
    debug(I, "Sending oseq for packet: %d\n", seq);
    sockiface(send_pdu)(sk, pdu, dst_addr);
  }
}

static void handle_sack(__tp(handler)* sk, __tp(pdu)* pdu) {
  nack_interval* i = (nack_interval*) pdu->data;

  i->lseq = ntohl(i->lseq);
  i->rseq = ntohl(i->rseq);

  control_data* cd = (control_data*) sk->transport.control_data;
  rt_connection* rt = get_connection(cd->ct, pdu->id, window);
  uint64_t oseq = produce_missing_packets(rt->buff, i, sk->outgoing_messages);

  //Do we need to send an oseq?
  if (oseq != -1)
    send_oseq(sk, pdu->id, oseq);
}

static void handle_oseq(__tp(handler)* sk, __tp(pdu)* pdu) {
  uint32_t oseq = *((uint32_t*) (pdu->data));
  oseq = ntohl(oseq);

  control_data* cd = (control_data*) sk->transport.control_data;
  rt_connection* rt = get_connection(cd->ct, pdu->id, window);

  debug(I, "Received oseq -> %d\n", oseq);
  report_oldest_sequence(rt->receive_buff, oseq);
}

static void send_sack(__tp(handler)* sk, nack_interval* i, uint16_t dst_hid) {
  __tp(pdu)* pdu =
      alloc_bytes(__tp(pdu), sizeof(__tp(pdu)) + sizeof(nack_interval));
  pdu->flags.sack = 1;
  pdu->id = sk->id;
  pdu->len = sizeof(nack_interval);
  i->lseq = htonl(i->lseq);
  i->rseq = htonl(i->rseq);
  memcpy(pdu->data, i, sizeof(nack_interval));

  int status;
  struct sockaddr dst_addr;
  if ((status = get_handler_address(dst_hid, &dst_addr,
      sk->module_communication.regd)) < 0) {
    log(E, "Unable to get destination address for app %d. Error: %s\n",
        dst_hid, registry_error_str(-status));
  } else {
    debug(I, "Sending sack [%d:%d]\n", ntohl(i->lseq), ntohl(i->rseq));
    sockiface(send_pdu)(sk, pdu, dst_addr);
  }
  free(i);
}

void __tp(reliable_send)(__tp(handler)* sk) {
  int status;
  struct sockaddr dst_addr;
  debug(I, "Reliable thread send initialized.\n");
  while (true) {
    msg* m = NULL;
    m = __tp(consume)(sk->outgoing_messages);
    connection_table* ct = ((control_data*) sk->transport.control_data)->ct;
    rt_connection* rt = get_connection(ct, m->dst_app_id, window);
    debug(I, "Before buffering this, I'll send it to %d\n", m->dst_app_id);
    add_packet(rt->buff, m);
    debug(I, "After buffering this, I'll send it to %d\n", m->dst_app_id);
    debug(I, "Got packet to send.\n");
    if ((status = get_handler_address(m->dst_app_id, &dst_addr,
        sk->module_communication.regd)) < 0) {
      log( E, "Unable to get destination address for app %d. Error: %s\n",
          m->dst_app_id, registry_error_str(-status));
    } else {
      sockiface(send_pdu)(sk, m->pdu, dst_addr);
      if (m->pdu->flags.con_terminate) {
        handle_con_terminate(sk, m->dst_app_id);
        debug(I, "Reliable transport closed.\n");
      }
    }
  }
}

/*
 * This function should be ran by a thread. It will probe every connection and send sacks and deliver packets.
 * */
static void time_slice(__tp(handler)* sk) {
  nack_interval* i;
  connection_table* ct = ((control_data*) sk->transport.control_data)->ct;
  while (1) {
    sleep(sack_timeout);
    control_data* cd = (control_data*) sk->transport.control_data;
    tree_iterator* it = new_tree_iterator(cd->ct);
    while (tree_iterator_has_next(it)) {
      rt_connection *c = (rt_connection*) tree_iterator_next(it);
      if (c->receive_buff) {
        debug(I, "DELIVERING AND POSSIBLY SENDING SACKS.\n");
        deliver_packets(c->receive_buff, sk);

        i = probe_missing_packets(c->receive_buff);
        if (i) {
          debug(I, "Sack timeout expired. Sending sack.\n");
          send_sack(sk, i, c->buff->hid);
        }
      }
    }
    destroy_iterator(it);
  }
}

void __tp(reliable_receive)(__tp(handler)* sk, __tp(pdu)* pdu) {
  if (pdu->flags.con_terminate) {
    handle_con_terminate(sk, pdu->id);
    return;
  }

  if (pdu->flags.sack) {
    handle_sack(sk, pdu);
    return;
  }

  if (pdu->flags.oseq) {
    handle_oseq(sk, pdu);
    return;
  }

  //Get connection and put the packet in the sent packets buffer
  connection_table* ct = ((control_data*) sk->transport.control_data)->ct;
  rt_connection* rt = get_connection(ct, pdu->id, window);
  setup_receiver(rt, recvsize);
  put_pkt(rt->receive_buff, sk, pdu);
}

void __tp(create_reliable_transport)(__tp(handler)* sk) {
  char* val;
  char* window_s;
  char* sack_timeout_s;
  char* recvsize_s;

  val = com_get_transport_configuration_parameter(
      sk->module_communication.confd, "reliable", "message_buffer_size");
  window_s = com_get_transport_configuration_parameter(
      sk->module_communication.confd, "reliable", "window");

  sack_timeout_s = com_get_transport_configuration_parameter(
      sk->module_communication.confd, "reliable", "synch_sack_time");

  recvsize_s = com_get_transport_configuration_parameter(
      sk->module_communication.confd, "reliable", "receive_buffer_size");

  window = DEFAULT_RELIABLE_TRANSPORT_WINDOW_SIZE;
  if (window_s) {
    window = atoi(window_s);
    free(window_s);
  }

  sack_timeout = DEFAULT_RELIABLE_TRANSPORT_SACK_TIMEOUT;
  if (sack_timeout_s) {
    sack_timeout = atoi(sack_timeout_s);
    free(sack_timeout_s);
  }

  recvsize = DEFAULT_RELIABLE_TRANSPORT_RECEIVE_BUFFER_SIZE;
  if (recvsize_s) {
    recvsize = atoi(sack_timeout_s);
    free(recvsize_s);
  }

  if (!val)
    sk->outgoing_messages = __tp(new_message_buffer)(
        DEFAULT_TRANSPORT_OUTGOING_MESSAGE_BUFFER_SIZE);
  else {
    int ival = atoi(val);
    debug(I, "Setting transport message buffer size to %d messages.\n", ival);
    sk->outgoing_messages = __tp(new_message_buffer)(ival);
    free(val);
  }

  if (!sk->outgoing_messages) {
    log(E, "Failed to initialize outgoing message buffer.\n");
    return;
  }

  sk->application.send_cb = schedule_message;
  sk->application.send_with_timestamp_cb = schedule_message_with_ts;
  sk->application.close_handler_cb = stop_reliable_transport;
  sk->transport.receive_cb = __tp(reliable_receive);

  control_data* cd = alloc(control_data, 1);
  pthread_mutex_init(&(cd->app_lock), NULL);
  pthread_mutex_lock(&(cd->app_lock)); //Down mutex asap. This will make other calls block on it
  cd->ct = mkconnection_table();
  pthread_create(&(cd->deliver), NULL, (void*) time_slice, sk);
  sk->transport.control_data = (void*) cd;
  pthread_create(&(sk->threads.sender), NULL, (void*) __tp(reliable_send),
      (void*) sk);
  pthread_create(&(sk->threads.receiver), NULL, (void*) sockiface(receive_pdu),
      (void*) sk);
}
