#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <fred/macros.h>

#include "cmd.h"


static int cmd_register_handler_callback(struct cmd* c, uint16_t hid, struct sockaddr addr){
  return register_handler(hid, addr, c->rd);
}

static int cmd_unregister_handler_callback(struct cmd* c, uint16_t hid){
  return unregister_handler_address(hid, c->rd);
}

static struct sockaddr* cmd_get_handler_callback(struct cmd* c, uint16_t hid){
  struct sockaddr* s = alloc(struct sockaddr, 1);
  int status = get_handler_address(hid, s, c->rd);
  if(status < 0){
    log(E, "%s", registry_error_str(-status));
    free(s);
    return NULL;
  }
  return s;
}

command* create_command(uint8_t type){
  command* c = alloc(command, 1);

  c->rd = make_registry_socket();
  if(!c->rd)
    return NULL;

  c->cmd_type = type;

  switch(type){
  case CMD_REGISTER:
    c->cmd.register_handler_address_cb = cmd_register_handler_callback;
    break;
  case CMD_UNREGISTER:
    c->cmd.unregister_handler_address_cb = cmd_unregister_handler_callback;
    break;
  case CMD_GET:
    c->cmd.get_handler_address_cb = cmd_get_handler_callback;
    break;
  default:
    log(E, "Unnable to create command for required type.\n");
    return NULL;
  }
  return c;
}

void free_command(command* c){
  if(!c)
    return;
  free(c);
}
