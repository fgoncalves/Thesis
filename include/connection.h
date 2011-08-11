#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "packet_buffer.h"
#include "recv_buff.h"

typedef struct rtcon{
	packet_buffer* buff;
	recv_buff* receive_buff;
}rt_connection;

extern rt_connection* mkrtconnection(uint32_t ssize, uint16_t hid);
extern void setup_receiver(rt_connection* c, uint32_t rsize);
extern void free_rtconnection(rt_connection* c);
#endif
