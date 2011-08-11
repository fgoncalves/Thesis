#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "macros.h"
#include "communication_types.h"
#include "communication.h"
#include "registry.h"

static int send_return_code(int code, int client_socket, struct sockaddr_un cl_addr, int cl_len){
  int status;
  registry_msg m;
    
  memset(&m, 0, sizeof(registry_msg));
  m.type = code;
  status = sendto(client_socket, &m, sizeof(registry_msg), 0, (struct sockaddr*) &cl_addr, cl_len);
  if(status == -1){
    log(E, "Failed to write return code to socket %d. Error: %s\n", client_socket, strerror(errno));
    return 0;
  }
  return 1;
}

uint8_t com_registry_register_handler(uint16_t id, struct sockaddr addr, int client_socket, struct sockaddr_un cl_addr, int cl_len){
  int status;

  if((status = registry_register_handler(id, addr))){
    send_return_code(status, client_socket, cl_addr, cl_len);
    return 0;
  }
  send_return_code(HERROR_NO_ERROR, client_socket, cl_addr, cl_len);
  return 1;
}

uint8_t com_registry_get_handler_address(uint16_t id, int client_socket, struct sockaddr_un cl_addr, int cl_len){
  struct sockaddr* addr = registry_get_handler_address(id);
  registry_msg m;
  int status;
  if(!addr){
    send_return_code(H_ERROR_NO_HANDLER, client_socket, cl_addr, cl_len);
    return 0;
  }

  memset(&m, 0, sizeof(registry_msg));
  m.type = HANDLER_ADDR;
  memcpy(&(m.addr), addr, sizeof(struct sockaddr));
  status = sendto(client_socket,  &m, sizeof(registry_msg), 0, (struct sockaddr*)&cl_addr, cl_len);
  if(status == -1){
    debug(E, "Unnable to send handler address to client. Error: %s\n", strerror(errno));
    return 0;
  }
  return 1;
}

uint8_t com_registry_unregister_handler(uint16_t id, int client_socket, struct sockaddr_un cl_addr, int cl_len){
  int status = registry_unregister_handler(id);
  if(status){
    send_return_code(status, client_socket, cl_addr, cl_len);
    return 0;
  }
  send_return_code(HERROR_NO_ERROR, client_socket, cl_addr, cl_len);
  return 1;
}
