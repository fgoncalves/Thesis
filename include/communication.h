#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

#include "communication_types.h"

extern uint8_t com_registry_register_handler(uint16_t id, struct sockaddr addr, int client_socket, struct sockaddr_un cl_addr, int cl_len);
extern uint8_t com_registry_get_handler_address(uint16_t id, int client_socket, struct sockaddr_un cl_addr, int cl_len);
extern uint8_t com_registry_unregister_handler(uint16_t id, int client_socket, struct sockaddr_un cl_addr, int cl_len);

#endif
