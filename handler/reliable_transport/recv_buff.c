#include <stdlib.h>
#include <string.h>
#include "recv_buff.h"
#include "message_buffer.h"
#include "macros.h"

#define lock_rv_buff(X) (pthread_mutex_lock(&((X)->lock)))
#define unlock_rv_buff(X) (pthread_mutex_unlock(&((X)->lock)))

recv_buff* mkrecv_buff(uint32_t size) {
  recv_buff* rb = alloc(recv_buff, 1);

  rb->buff = alloc(__tp(pdu)*, size);
  rb->len = size;
  pthread_mutex_init(&(rb->lock), NULL);
  return rb;
}

void free_recv_buff(recv_buff* rv) {
  if (rv) {
    pthread_mutex_destroy(&(rv->lock));
    free(rv->buff);
    free(rv);
  }
}

void print_recv_buff(recv_buff* rv) {
  int i;
  printf("[ ");
  lock_rv_buff(rv);
  for (i = 0; i < rv->len; i++) {
    if ((rv->buff)[i])
      printf("%d ", (rv->buff)[i]->id);
    else
      printf("NULL ");
  }unlock_rv_buff(rv);
  putchar('\n');
}

nack_interval* probe_missing_packets(recv_buff* rv) {
  int i, found = 0, j;
  nack_interval* inter;

  //Check if everything is null
  lock_rv_buff(rv);
  for (i = 0; i < rv->len; i++)
    if ((rv->buff[i])) {
      found = 1;
      break;
    }

  if (!found) {
    debug(I, "Missing all packets.\n");
    inter = alloc(nack_interval, 1);
    inter->lseq = rv->how_many_delivered;
    inter->rseq = rv->how_many_delivered + rv->len - 1;
    unlock_rv_buff(rv);
    return inter;
  }

  //Check if buffer is full
  for (i = 0, found = 0; i < rv->len; i++)
    if (!(rv->buff[i])) {
      found = 1;
      break;
    }

  if (!found) {
    unlock_rv_buff(rv);
    debug(I, "There are no packets missing.\n");
    return NULL;
  }

  int inicio = rv->how_many_delivered % rv->len;

  for (i = 0; i < rv->len; i++)
    if (!(rv->buff)[(i + inicio) % rv->len])
      break;

  int start = i, end;

  for (j = i; j < rv->len; j++)
    if ((rv->buff)[(j + inicio) % rv->len])
      break;

  //Lookout for overflow
  if (j == rv->len)
    j--;

  if ((rv->buff)[(j + inicio) % rv->len]) {
    end = (rv->buff)[(j + inicio) % rv->len]->seq - 1;
    start = end - (j - start) + 1;
  } else {
    start = (rv->buff)[(i + inicio) % rv->len - 1]->seq + 1;
    end = start + (rv->len - i) - 1;
  }

  inter = alloc(nack_interval, 1);
  inter->lseq = start;
  inter->rseq = end;
  debug(I, "Missing packets [%d:%d].\n", inter->lseq, inter->rseq);unlock_rv_buff(
      rv);
  return inter;
}

//Not thread safe
static uint8_t deliver_and_add(recv_buff* rv, __tp(handler)* sk, __tp(pdu)* pkt) {
  if (!(rv->buff)[rv->how_many_delivered % rv->len]) {
    return 0;
  }

  int start = rv->how_many_delivered % rv->len, count = 0, i;
  debug(I, "Delivering packets. Starting at %d\n", start);
  for (i = 0; i < rv->len; i++, count++, start++, start %= rv->len) {
    if (!(rv->buff)[start])
      break;
    if (sk->application.receive_cb) {
      __tp(pdu)* pdu = (rv->buff)[start];
      debug(I, "Delivering packet %d\n", pdu->seq);
      sk->application.receive_cb(sk, pdu->data, pdu->len, pdu->timestamp,
          pdu->rtt, pdu->id);
    }
    (rv->buff)[start] = NULL;
  }debug(I, "Finished delivery process.\n");
  rv->how_many_delivered += count;
  if (pkt->seq - rv->how_many_delivered >= rv->len) {
    debug(I, "Buffer is too short to hold pdu %d\n", pkt->id);
    return 0;
  }

  (rv->buff)[pkt->seq % rv->len] = pkt;
  return 1;
}

void put_pkt(recv_buff* rv, __tp(handler)* sk, __tp(pdu)* pdu) {
  lock_rv_buff(rv);
  if (pdu->seq < rv->how_many_delivered) {
    unlock_rv_buff(rv);
    debug(I, "Duplicated packet\n");
    return;
  }

  if (pdu->seq - rv->how_many_delivered >= rv->len) {
    deliver_and_add(rv, sk, pdu);
    unlock_rv_buff(rv);
    return;
  }

  (rv->buff)[pdu->seq % rv->len] = pdu;
  unlock_rv_buff(rv);
  return;
}

void report_oldest_sequence(recv_buff* rv, uint32_t oldest) {
  lock_rv_buff(rv);
  if (rv->how_many_delivered < oldest)
    rv->how_many_delivered = oldest;
  unlock_rv_buff(rv);
}

uint8_t deliver_packets(recv_buff* rv, __tp(handler)* sk) {
  lock_rv_buff(rv);
  if (!(rv->buff)[rv->how_many_delivered % rv->len]) {
    unlock_rv_buff(rv);
    return 0;
  }

  int start = rv->how_many_delivered % rv->len, count = 0, i;
  debug(I, "Delivering packets. Starting at %d\n", start);
  for (i = 0; i < rv->len; i++, count++, start++, start %= rv->len) {
    if (!(rv->buff)[start])
      break;
    if (sk->application.receive_cb) {
      __tp(pdu)* pdu = (rv->buff)[start];
      debug(I, "Delivering packet %d\n", pdu->seq);
      sk->application.receive_cb(sk, pdu->data, pdu->len, pdu->timestamp,
          pdu->rtt, pdu->id);
    }
    (rv->buff)[start] = NULL;
  }debug(I, "Finished delivery process.\n");
  rv->how_many_delivered += count;
  unlock_rv_buff(rv);
  return 1;
}
