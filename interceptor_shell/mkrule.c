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

	if(!write_to_proc('R', pa->iname, pa->proto, pa->daddr, pa->saddr, pa->dport, pa->sport, NULL)){
		printf("Failed to create rule.\n");
		return -1;
	}
	return 0;
}
