#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <fred/handler.h>

int fd = -1;

void receive_cb(__tp(handler)* sk, char* data, uint16_t len, int64_t ts, int64_t air, uint16_t hid){
  int status;

  status = write(fd, data, len);
  if(status == -1)
    perror("Failed to write to file");
}

int main(int argc, char** argv){
  if(argc != 2){
    printf("Usage: %s <config_file>\n", argv[0]);
    return 1;
  }

  fd = open("temp.data", O_CREAT|O_TRUNC|O_WRONLY);
  if(fd < 0){
    perror("Failed to open file temp.data");
    return 1;
  }
  __tp(handler)* sk = __tp(create_handler_based_on_file)(argv[1], receive_cb);

  char c;
  scanf("%c", &c);
  close(fd);
  printf("Finished.\n");
  return 0;
}
