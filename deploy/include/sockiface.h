/************************************************************
 * This file and the correspondent .c exist only to serve   *
 * the architecture. Functions declared in it should never  *
 * be used by applications.                                 *
 ************************************************************/
#ifndef __SOCKIFACE_H__
#define __SOCKIFACE_H__

#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include "transport.h"
#include "pdu.h"
#include "handler.h"

#define sockiface(NAME) socket_interface_ ## NAME

extern int8_t sockiface(send_pdu)(__tp(handler)* h, __tp(pdu)* p, struct sockaddr dst);
extern void sockiface(receive_pdu)(__tp(handler)* handler);

#endif
