#include <string.h>
#include <errno.h>
#include "communication.h"
#include "exec.h"
#include "macros.h"

void serve_request(registry_msg* r, int cl_fd, struct sockaddr_un cl_addr, int cl_len){
  switch(r->type){
  case HANDLER_GET:
    com_registry_get_handler_address(r->id, cl_fd, cl_addr, cl_len);
    break;
  case HANDLER_REGISTER:
    com_registry_register_handler(r->id, r->addr, cl_fd, cl_addr, cl_len);
    break;
  case HANDLER_UNREGISTER:
    com_registry_unregister_handler(r->id, cl_fd, cl_addr, cl_len);
    break;
  default:{
    registry_msg ans;
    int status;
    memset(&ans, 0, sizeof(registry_msg));
    ans.type = HERROR_UNKNOWN_REQ;
    status = sendto(cl_fd, &ans, sizeof(registry_msg), 0, (struct sockaddr*) &cl_addr, cl_len);
    if(status == -1)
      log(E, "Unnable to send data to client. Error: %s\n", strerror(errno));
  }
  }
}
