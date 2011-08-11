#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdint.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fred/handler.h>

#define LEN 4

int in;

void run(char* f, uint16_t hid){
  __tp(handler)* sk = __tp(create_handler_based_on_file)(f, NULL);
 
  char buff[LEN];
  memset(buff, 0, LEN);
  int l;

  while((l = read(in, buff, LEN)) > 0){
    send_data(sk, buff, l, hid);
    memset(buff, 0, LEN);
  }

  printf("Data has been sent\n");
  close_transport_handler(sk);
  close(in);
}

int main(int argc, char** argv){
  if(argc != 3){
    printf("Usage: %s <config_file> <dst_hid>\n", argv[0]);
    return 1;
  }

  in = open("in", O_RDONLY);
  if(in < 0){
    perror("Failed to open file in");
    return 1;
  }
  run(argv[1], atoi(argv[2]));
  return 0;
}
