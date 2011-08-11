#ifndef __PDU_TRANSPORT_FCNTL_H__
#define __PDU_TRANSPORT_FCNTL_H__

#include "pdu.h"

extern void __tp(synch_pdu)(__tp(pdu)*, int64_t ts);
extern void __tp(free_pdu)(__tp(pdu)*);
extern __tp(pdu)* __tp(new_pdu)(char* data, uint32_t len);
#endif
