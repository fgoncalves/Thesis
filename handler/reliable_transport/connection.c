#include <stdlib.h>
#include "macros.h"
#include "connection.h"

rt_connection* mkrtconnection(uint32_t size, uint16_t hid){
	rt_connection* c = alloc(rt_connection, 1);
	c->buff = mkpacket_buffer(size, hid);
	return c;
}

void setup_receiver(rt_connection* c, uint32_t rsize){
  if(!c->receive_buff){
    c->receive_buff = mkrecv_buff(rsize);
  }
}

void free_rtconnection(rt_connection* c){
	if(c){
		free_packet_buffer(c->buff);
		free_recv_buff(c->receive_buff);
		free(c);
	}
}
