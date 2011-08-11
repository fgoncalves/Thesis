#include <stdlib.h>
#include "macros.h"
#include "nack.h"
#include "packet_buffer.h"

#define lock_buffer(X) (pthread_mutex_lock(&((X)->lock)))
#define unlock_buffer(X) (pthread_mutex_unlock(&((X)->lock)))

packet_buffer* mkpacket_buffer(uint32_t size, uint16_t hid) {
  packet_buffer* pb = alloc(packet_buffer, 1);

  pb->size = size;
  pb->buffer = alloc(msg*, size);
  pb->hid = hid;
  pb->largest_seq = -1;
  pb->oldest_seq = 0;
  pthread_mutex_init(&(pb->lock), NULL);

  return pb;
}

void free_packet_buffer(packet_buffer* pb) {
  uint32_t i;
  if (pb) {
    lock_buffer(pb);
    for (i = 0; i < pb->size; i++)
      if ((pb->buffer)[i]){
        free((pb->buffer[i])->pdu);
        free((pb->buffer)[i]);
      }
    free(pb->buffer);
    unlock_buffer(pb);
    pthread_mutex_destroy(&(pb->lock));
    free(pb);
  }
}

void add_packet(packet_buffer* pb, msg* m) {
  lock_buffer(pb);
  if (m->pdu->seq <= pb->largest_seq) {
    unlock_buffer(pb);
    debug(I, "Packet has already been buffered.\n");
    return;
  }
  if (m->pdu->seq > pb->largest_seq)
    pb->largest_seq = m->pdu->seq;

  if ((m->pdu->seq % pb->size == pb->oldest_seq % pb->size)
      && (m->pdu->seq != pb->oldest_seq)){
    if((pb->buffer)[pb->oldest_seq % pb->size]){
       free((pb->buffer)[pb->oldest_seq % pb->size]->pdu);
       free((pb->buffer)[pb->oldest_seq % pb->size]);
    }
    pb->oldest_seq++;
  }
  (pb->buffer)[m->pdu->seq % pb->size] = m;
  unlock_buffer(pb);
}

/*
 * Return -1 in case of no packets missing.
 * Return the oldest sequence number available.
 * */
int64_t produce_missing_packets(packet_buffer* pb, nack_interval* nack,
    __tp(buffer)* b) {
  int retval = -1;
  if (!nack)
    return -1;

  debug(I, "Received ACK - [%d:%d]\n", nack->lseq, nack->rseq);

  lock_buffer(pb);
  uint32_t oldest = pb->oldest_seq;
  uint32_t latest = pb->largest_seq;
  int lseq_pos, rseq_pos, how_many_to_produce, i;

  //We do not have any packets buffered. They're too old.
  if (oldest > nack->rseq) {
    debug(W, "Requested %d packet not in buffer. Oldest %d\n", nack->rseq, oldest);
    unlock_buffer(pb);
    return oldest;
  }

  //We are being asked for packets we do not have
  if(latest < nack->lseq){
    debug(I, "We do not have those packets.\n");
    unlock_buffer(pb);
    return -1;
  }

  //We do not have all packets buffered. Some are too old.
  if (oldest > nack->lseq) {
    debug(W, "Requested %d packet not in buffer. Oldest %d\n", nack->lseq, oldest);
    retval = lseq_pos = oldest;
  } else
    lseq_pos = pb->oldest_seq % pb->size + (nack->lseq - oldest);

  //This is a case where we've been ask for packets we do not have.
  if (latest < nack->rseq) {
    debug(W, "Asked for packets we do not have. Latest is %d\n", latest);
#ifdef DEBUG
    int i;
    printf("[ ");
    for (i = 0; i < pb->size; i++)
      if ((pb->buffer)[(pb->oldest_seq % pb->size + i) % pb->size])
        printf("%d ", ((pb->buffer)[(pb->oldest_seq % pb->size + i) % pb->size])->pdu->seq);
    printf("]\n");
#endif
    rseq_pos = latest;
  } else
    rseq_pos = nack->rseq;

  how_many_to_produce = (rseq_pos - lseq_pos) + 1;
  debug(I, "Producing %d packets...\n", how_many_to_produce);
  for (i = 0; i < how_many_to_produce; i++) {
    debug(I, "Producing packet %d.\n", (pb->buffer)[(lseq_pos + i) % pb->size]->pdu->seq);
    if (!__tp(try_produce)(b, (pb->buffer)[(lseq_pos + i) % pb->size])) {
      debug(I, "Producers buffer was full. I could not produce all packets.\n");
      break;
    }
  }
  unlock_buffer(pb);
  return retval;
}
