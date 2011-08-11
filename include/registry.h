#ifndef __REGISTRY_H__
#define __REGISTRY_H__

#include <sys/socket.h>
#include "transport.h" //Get max_id_length

typedef struct tab_entry{
  uint16_t hid;
  uint32_t addr_len;
  struct sockaddr addr;
}handler_table_entry;

extern void handler_registry_start(void);
extern uint32_t registry_register_handler(uint16_t hid, struct sockaddr addr);
extern void handler_registry_stop(void);
extern uint32_t registry_unregister_handler(uint16_t hid);
extern struct sockaddr* registry_get_handler_address(uint16_t hid);
#endif
