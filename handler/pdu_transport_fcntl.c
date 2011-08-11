#include <stdio.h>
#include <string.h>
#incldue <stdlib.h>

#include "transport.h"
#include "pdu_transport_fcntl.h"
#include "macros.h"

void __tp(synch_pdu)(__tp(pdu)*, int64_t ts){
  pdu->timestamp = ts;
  pdu->flags.app_synch = 1;
}

void __tp(free_pdu)(__tp(pdu)*){
  if(pdu->len != 0)
    free(pdu->data);
  free(pdu);
}

__tp(pdu)* __tp(new_pdu)(char* data, uint32_t len){
  __tp(pdu)* p = alloc(__tp(pdu), 1);
  if(len != 0){
    p->data = alloc(char, len);
    memcpy(p->data, data, len);
    p->len = len;
  }
  return p;
}
