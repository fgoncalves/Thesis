#include <pthread.h>
#include <stdint.h>
#include "sockiface.h"
#include "macros.h"
#include "timestamp.h"
#include "unreliable_transport.h"
#include "registry_ops.h"
#include "registry_error.h"

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

static int8_t schedule_message_with_ts(__tp(handler)* sk, char* data, uint16_t len,
		uint16_t dst_app, int64_t timestamp) {
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

static int8_t stop_unreliable_transport(__tp(handler)* sk) {
	//unreliable transport does not need to be stoped
	return 1;
}

void __tp(unreliable_send)(__tp(handler)* sk) {
	int status;

	while (true) {
		msg* m = __tp(consume)(sk->outgoing_messages);
		struct sockaddr dst_addr;

		if ((status = get_handler_address(m->dst_app_id, &dst_addr,
				sk->module_communication.regd)) < 0) {
			log(E, "Unable to get destination address for app %d. Error: %s\n", m->dst_app_id, registry_error_str(-status));
		} else {
			sockiface(send_pdu)(sk, m->pdu, dst_addr);
		}

		free(m->pdu);
		free(m);
	}
}

void __tp(unreliable_receive)(__tp(handler)* sk, __tp(pdu)* pdu) {
	if(sk->application.receive_cb)
		sk->application.receive_cb(sk, pdu->data,
				pdu->len, pdu->timestamp, pdu->rtt, pdu->id);
}

void __tp(create_unreliable_transport)(__tp(handler)* sk) {
	char* val;

	val = com_get_transport_configuration_parameter(sk->module_communication.confd, "unreliable", "message_buffer_size");

	if(!val)
	sk->outgoing_messages = __tp(new_message_buffer)(
			DEFAULT_TRANSPORT_OUTGOING_MESSAGE_BUFFER_SIZE);
	else{
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
	sk->application.close_handler_cb = stop_unreliable_transport;
	sk->transport.receive_cb = __tp(unreliable_receive);
	pthread_create(&(sk->threads.sender), NULL, (void*) __tp(unreliable_send),
			(void*) sk);
	pthread_create(&(sk->threads.receiver), NULL,
			(void*) sockiface(receive_pdu), (void*) sk);
}
