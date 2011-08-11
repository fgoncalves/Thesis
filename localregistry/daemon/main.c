#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <fred/exec.h>
#include <fred/macros.h>

#define MAX_PENDING_REQUESTS 5

registry_msg *m = NULL;

void cleanup_daemon(){
  handler_registry_stop();
  if(m)
    free(m);
  exit(0);
}

void init_daemon(){
  handler_registry_start();
  signal(SIGINT, cleanup_daemon);
  signal(SIGTERM, cleanup_daemon);
}

void serve(){
  int server_sockfd, server_len, client_len = sizeof(struct sockaddr_un), status, on = 1;
  struct sockaddr_un server_address, client_address;

  unlink("/tmp/registry_daemon");
  server_sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
  if(server_sockfd == -1){
    log(E, "Unnale to create socket. Error: %s\n", strerror(errno));
    return;
  }

  memset(&server_address, 0, sizeof(server_address));
  memset(&client_address, 0, sizeof(client_address));
  server_address.sun_family = AF_LOCAL;
  strcpy(server_address.sun_path, "/tmp/registry_daemon");
  server_len = sizeof(server_address);
  bind(server_sockfd, (struct sockaddr*)&server_address, server_len);

  status = setsockopt(server_sockfd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
  if(status == -1){
    log(E, "Setsockopt failed. Error: %s\n", strerror(errno));
    return;
  }

  while(true){
    m = alloc(registry_msg, 1);
    status = recvfrom(server_sockfd, m, sizeof(registry_msg), 0, (struct sockaddr*)&client_address, &client_len);
    if(status < 0){
      log(E, "Read returned and error. Error: %s\n", strerror(errno));
      continue;
    }
    serve_request(m, server_sockfd, client_address, client_len);
    free(m);
    m = NULL;
  }
}

int main(){
  init_daemon();
  serve();
}
