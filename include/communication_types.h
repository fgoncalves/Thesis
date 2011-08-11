#ifndef __COMMUNICATION_TYPES_H__
#define __COMMUNICATION_TYPES_H__

#include <sys/un.h>
#include <stdint.h>
#include "registry.h"
#include "registry_error.h"

/* This enum defines the operations provided by this daemon
 */
enum{
  HERROR_NO_ERROR,
  HANDLER_REGISTER = 2024,
  HANDLER_GET,
  HANDLER_UNREGISTER,
  HERROR_CODE,
  HANDLER_ADDR,
  HERROR_UNKNOWN_REQ,
};

typedef struct{
  struct sockaddr addr;
  uint32_t type;
  uint16_t id;
}registry_msg;
#endif
