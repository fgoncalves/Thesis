#ifndef __DATA_BUFFER_H__
#define __DATA_BUFFER_H__

#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>
#include "pdu.h"
#include "transport.h"

typedef struct{
	int8_t dst_app_id;
	__tp(pdu)* pdu;
}msg;

typedef struct __tp(mbuffer){
  pthread_mutex_t lock;
  sem_t empty;
  sem_t full;
  uint32_t buff_size; //number of data packets the buffer may have
  uint32_t itens;
  uint32_t consumer_pointer;
  uint32_t producer_pointer;
  
  msg** data; //array of pointers to messages
}__tp(buffer);


//Warning:: This is not thread safe and should only be called when there are no threads locked in the buffer.
extern void __tp(destroy_buffer)(__tp(buffer) *b);
//------------------------------------------------

extern msg* __tp(remove_message)(__tp(buffer)*  b, int offset);
extern __tp(buffer)* __tp(new_message_buffer)(uint32_t npackets);
extern msg* __tp(consume) (__tp(buffer)* b);
extern msg* __tp(peek) (__tp(buffer)* b, uint32_t offset);
extern void __tp(produce)(__tp(buffer)* b, msg* m);

#endif
