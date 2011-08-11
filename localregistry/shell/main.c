#include <stdio.h>
#include <stdint.h>
#include "argparse.h"

void print_help(char* cmd){
  printf("Usage: %s <get|register|unregister> hid [<ip> <port>] [<unix_path>]\n", cmd);
  printf("\n");
  printf("\t%s G <hid>\n", cmd);
  printf("\t%s R <hid> [<ip> <port>] [<unix_path>]\n", cmd);
  printf("\t%s U <hid>\n", cmd);
  printf("\n");
  printf("NOTE: Names may be in lower case.\n", cmd);
}

int main(int argc, char** argv){
  if(!parse_args_and_execute(argv, argc))
    print_help(argv[0]);
  return 0;
}
		 
