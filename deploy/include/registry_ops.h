#ifndef __REGISTRY_OPS_H__
#define __REGISTRY_OPS_H__

#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>

#include "registry_error.h"

typedef struct{
  int sockfd;
  struct sockaddr_un addr;
}registry_descriptor; 

extern registry_descriptor* make_registry_socket();
extern void free_registry_socket(registry_descriptor* fd);
/*On success returns 1. If Failed returns < 0, and one should use the function registry_error_str to be able to know what happen.*/
extern int register_handler(uint16_t id, struct sockaddr haddr, registry_descriptor* fd);
extern int get_handler_address(uint16_t id, struct sockaddr* haddr_out, registry_descriptor* fd);
extern int unregister_handler_address(uint16_t id, registry_descriptor* fd);
#endif
