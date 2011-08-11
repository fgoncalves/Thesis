#ifndef __MSG_H__
#define __MSG_H__

#include "transport.h"

//This is used to tell interceptors which kind of paylod the packet has. It should only be used by interceptors
typedef struct __attribute__ ((__packed__)) int_control_byte {
	uint8_t synched :1, app_aggregated :1, l3_aggregated :1, deaggregated :1,
			unassigned :4;
} control_byte;

#define PDU_MAX_PAYLOAD_SIZE (1024 - sizeof(__tp(pdu)))

#define MAX_PDU_PRIO 0xFF
#define MIN_PDU_PRIO 0x00

typedef struct __tp(msg) {
	int64_t timestamp;
	int64_t rtt;
	uint32_t seq;
	uint16_t len;
	uint16_t id;
	struct {
		uint8_t sack :1, app_synch :1, oseq :1, con_terminate :1, reserved :4;
	} flags;
	uint8_t prio;
	char data[0];
}__tp(pdu);

static inline int64_t swap_timestamp_byte_order(int64_t ts) {
	int64_t res;
	int32_t word;
	memcpy(&word, &ts, 4);
	word = htonl(word);
	memcpy(&res, &word, 4);
	memcpy(&word, ((char*) &ts) + 4, 4);
	word = htonl(word);
	memcpy(((char*) &res) + 4, &word, 4);
	return res;
}

static inline void swap_pdu_byte_order(__tp(pdu)* m) {
	m->timestamp = swap_timestamp_byte_order(m->timestamp);
	m->rtt = swap_timestamp_byte_order(m->rtt);
	m->len = htons(m->len);
	m->seq = htonl(m->seq);
	m->id = htons(m->id);
}

#endif
