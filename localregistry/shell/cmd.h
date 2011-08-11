#ifndef __CMD_H__
#define __CMD_H__

#include <fred/registry_ops.h>

#define CMD_REGISTER 1
#define CMD_UNREGISTER 2
#define CMD_GET 3

typedef struct cmd{
  registry_descriptor* rd;
  uint8_t cmd_type;
  union{
    struct sockaddr* (*get_handler_address_cb)(struct cmd* c, uint16_t hid);
    int (*register_handler_address_cb)(struct cmd* c, uint16_t hid, struct sockaddr addr);
    int (*unregister_handler_address_cb)(struct cmd* c, uint16_t hid);
  }cmd;
}command;

extern command* create_command(uint8_t type);
extern void free_command(command*);
#endif
