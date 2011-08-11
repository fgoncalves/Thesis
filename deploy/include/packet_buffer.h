#ifndef __PACKET_BUFFER_H__
#define __PACKET_BUFFER_H__

#include <stdint.h>
#include <string.h>
#include "pdu.h"
#include "message_buffer.h"

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
	uint32_t current;
	//Keep track of oldest packet in buffer so we can easily tell if we have a requested packet
	uint32_t oldest;
	uint32_t latest_plus_one; //Where should we put the next packet. This should not overlap current.
	uint32_t size;
	pthread_mutex_t lock;
} packet_buffer;

extern packet_buffer* mkpacket_buffer(uint32_t size, uint16_t hid);
extern void free_packet_buffer(packet_buffer*);

extern void add_packet(packet_buffer* pb, msg* m);

extern msg* get_next_packet_to_send(packet_buffer* pb);
extern uint32_t set_buffer_pointer_to(packet_buffer* pb, uint32_t seq);
#endif
