#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include "communication_types.h"
#include "registry_ops.h"
#include "macros.h"

registry_descriptor* make_registry_socket(){
  int len, status, on = 1;
  registry_descriptor* fd = alloc(registry_descriptor, 1);

  fd->sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);

  fd->addr.sun_family = AF_LOCAL;
  strcpy(fd->addr.sun_path, "/tmp/registry_daemon");
  len = sizeof(struct sockaddr_un);

  status = setsockopt(fd->sockfd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
  if(status == -1){
    log(E, "Setsockopt failed. Error: %s\n", strerror(errno));
    return NULL;
  }

  return fd;
}

void free_registry_socket(registry_descriptor* fd){
  close(fd->sockfd);
  free(fd);
}

int register_handler(uint16_t id, struct sockaddr haddr, registry_descriptor* fd){
  registry_msg answer, request;
  int status, len;

  memset(&request, 0, sizeof(registry_msg));
  memset(&answer, 0, sizeof(registry_msg));
    
  request.type = HANDLER_REGISTER;
  request.addr = haddr;
  request.id = id;

  status = sendto(fd->sockfd, &request, sizeof(registry_msg), 0, (struct sockaddr*) &(fd->addr), sizeof(struct sockaddr_un));
  if(status == -1){
    return -errno;
  }

  len = sizeof(struct sockaddr_un);
  status = recvfrom(fd->sockfd, &answer, sizeof(registry_msg), 0, (struct sockaddr*) &(fd->addr), &len);
  if(status == -1){
    return -errno;
  }

  if(answer.type != HERROR_NO_ERROR){
    return -answer.type;
  }
  return 1;
}

int get_handler_address(uint16_t id, struct sockaddr* haddr_out, registry_descriptor* fd){
  registry_msg answer, request;
  int status, len;

  memset(&request, 0, sizeof(registry_msg));
  memset(&answer, 0, sizeof(registry_msg));

  request.type = HANDLER_GET;
  request.id = id;

  status = sendto(fd->sockfd, &request, sizeof(registry_msg), 0, (struct sockaddr*) &(fd->addr), sizeof(struct sockaddr_un));
  if(status == -1){
    return -errno;
  }

  len = sizeof(struct sockaddr_un);
  status = recvfrom(fd->sockfd, &answer, sizeof(registry_msg), 0, (struct sockaddr*) &(fd->addr), &len);
  if(status == -1){
    return -errno;
  }

  if(answer.type != HANDLER_ADDR){
    return -answer.type;
  }

  memcpy(haddr_out, &(answer.addr), sizeof(struct sockaddr));
  return 1;
}

int unregister_handler_address(uint16_t id, registry_descriptor* fd){
  registry_msg answer, request;
  int status, len;

  memset(&request, 0, sizeof(registry_msg));
  memset(&answer, 0, sizeof(registry_msg));

  request.type = HANDLER_UNREGISTER;
  request.id = id;

  status = sendto(fd->sockfd, &request, sizeof(registry_msg), 0, (struct sockaddr*) &(fd->addr), sizeof(struct sockaddr_un));
  if(status == -1){
    return -errno;
  }

  len = sizeof(struct sockaddr_un);
  status = recvfrom(fd->sockfd, &answer, sizeof(registry_msg), 0, (struct sockaddr*) &(fd->addr), &len);
  if(status == -1){
    return -errno;
  }

  if(answer.type != HERROR_NO_ERROR){
    return -answer.type;
  }
  return 1;
}
