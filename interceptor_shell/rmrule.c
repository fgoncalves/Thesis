#include <stdlib.h>
#include <stdio.h>
#include "operations.h"
#include "cmd_line_parser.h"

int main(int argc, char** argv){
	parsed_args* pa;

	pa = parse_args(argc, argv);
	if(!pa){
		print_usage(argv[0]);
		return -1;
	}

	if(!write_to_proc('U', NULL, NULL, NULL, NULL, NULL, NULL, pa->ruleid)){
		printf("Failed to remove rule.\n");
		return -1;
	}
	return 0;
}
