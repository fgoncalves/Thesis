#include <string.h>
#include "macros.h"
#include "message_buffer.h"

__tp(buffer)* __tp(new_message_buffer)(uint32_t npackets) {
  __tp(buffer)* buff = alloc(__tp(buffer), 1);

  pthread_mutex_init(&(buff->lock), NULL);
  sem_init(&(buff->full), 0, npackets);
  sem_init(&(buff->empty), 0, 0);

  buff->buff_size = npackets;
  buff->consumer_pointer = 0;
  buff->producer_pointer = 0;
  buff->itens = 0;

  buff->data = alloc(msg*, npackets);

  return buff;
}

msg* __tp(consume)(__tp(buffer)* b) {
  msg* item;
  sem_wait(&(b->empty));
  pthread_mutex_lock(&(b->lock));

  item = (b->data)[b->consumer_pointer];
  b->consumer_pointer++;

  if (b->consumer_pointer >= b->buff_size)
    b->consumer_pointer = 0;

  b->itens--;

  pthread_mutex_unlock(&(b->lock));
  sem_post(&(b->full));
  return item;
}

msg* __tp(try_consume)(__tp(buffer)* b) {
  msg* item;
  if (sem_trywait(&(b->empty)))
    return NULL;
  pthread_mutex_lock(&(b->lock));

  item = (b->data)[b->consumer_pointer];
  b->consumer_pointer++;

  if (b->consumer_pointer >= b->buff_size)
    b->consumer_pointer = 0;

  b->itens--;

  pthread_mutex_unlock(&(b->lock));
  sem_post(&(b->full));
  return item;
}

void __tp(produce)(__tp(buffer)* b, msg* m) {
  sem_wait(&(b->full));
  pthread_mutex_lock(&(b->lock));

  (b->data)[b->producer_pointer] = m;
  b->producer_pointer++;

  if (b->producer_pointer >= b->buff_size)
    b->producer_pointer = 0;

  b->itens++;

  pthread_mutex_unlock(&(b->lock));
  sem_post(&(b->empty));
}

uint8_t __tp(try_produce)(__tp(buffer)* b, msg* m) {
  if (sem_trywait(&(b->full)))
    return 0;
  pthread_mutex_lock(&(b->lock));

  (b->data)[b->producer_pointer] = m;
  b->producer_pointer++;

  if (b->producer_pointer >= b->buff_size)
    b->producer_pointer = 0;

  b->itens++;

  pthread_mutex_unlock(&(b->lock));
  sem_post(&(b->empty));
  return 1;
}

void __tp(destroy_buffer)(__tp(buffer) *b) {
  uint32_t i, n;
  pthread_mutex_destroy(&(b->lock));
  sem_destroy(&(b->empty));
  sem_destroy(&(b->full));
  for (i = b->consumer_pointer, n = 0; n < b->itens; i++, n++) {
    if (i == b->buff_size)
      i = 0;
    msg* im = (b->data)[i];
    free(im->pdu);
    free(im);
  }
  free(b->data);
  free(b);
}
