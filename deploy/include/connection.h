#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "packet_buffer.h"

typedef struct rtcon{
	int64_t last_received_seq;
	packet_buffer* buff;
}rt_connection;

extern rt_connection* mkrtconnection(uint32_t size, uint16_t hid);
extern void free_rtconnection(rt_connection* c);
#endif
