#ifndef __PACKET_BUFFER_H__
#define __PACKET_BUFFER_H__

#include <stdint.h>
#include <string.h>
#include "pdu.h"
#include "message_buffer.h"
#include "nack.h"

/*
 * This structure is suppose to identify a connection between 2 nodes. Source and sink.
 *
 * The field buffer should contain an array of pointers to internal copies of ip packets.
 * This buffer is circular.
 *
 * Each time a packet is stored on it, an internal copy is made and the buffer pointer incremented.
 * */

typedef struct __pb {
	uint16_t hid;
	msg**buffer;
	int64_t largest_seq;
	uint32_t oldest_seq;
	int32_t size;
	pthread_mutex_t lock;
} packet_buffer;

extern packet_buffer* mkpacket_buffer(uint32_t size, uint16_t hid);
extern void free_packet_buffer(packet_buffer*);

extern void add_packet(packet_buffer* pb, msg* m);

extern int64_t produce_missing_packets(packet_buffer* pb, nack_interval* nack,
    __tp(buffer)* b);
#endif
