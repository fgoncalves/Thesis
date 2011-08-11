#ifndef __RECV_BUFF_H__
#define __RECV_BUFF_H__

#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "pdu.h"
#include "nack.h"
#include "handler.h"

typedef struct {
  pthread_mutex_t lock;
	uint32_t len;
	uint32_t how_many_delivered;
	__tp(pdu)** buff;
} recv_buff;

extern recv_buff* mkrecv_buff(uint32_t size);
extern void free_recv_buff(recv_buff* rv);
extern void print_recv_buff(recv_buff* rv);
extern void put_pkt(recv_buff* rv, __tp(handler)* sk, __tp(pdu)* pdu);
extern void report_oldest_sequence(recv_buff* rv, uint32_t oldest);
extern uint8_t deliver_packets(recv_buff* rv, __tp(handler)* sk);
extern nack_interval* probe_missing_packets(recv_buff* rv);
#endif
