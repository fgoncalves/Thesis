#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cmd_line_parser.h"

void print_usage(char* cmd) {
	printf(
			"Usage: %s [-n iname|-id rule_id] [-p protocol] [-da destinastion_address] [-sa source_address] [-dp destination_port] [-sp source_port]\n",
			cmd);
}

parsed_args* parse_args(int argc, char** argv) {
	int32_t i;

	parsed_args* pa = malloc(sizeof(parsed_args));
	if (!pa) {
		printf("Failed to parse arguments. No memory left.\n");
		return NULL;
	}

	memset(pa, 0, sizeof(parsed_args));

	if (argc == 1 || argc % 2 == 0) {
		printf("Invalid number of arguments.\n");
		return NULL;
	}

	for (i = 1; i < argc; i += 2) {
		if (!strcmp("-n", argv[i])) {
			pa->iname = argv[i + 1];
			continue;
		}
		if (!strcmp("-p", argv[i])) {
			pa->proto = argv[i + 1];
			continue;
		}
		if (!strcmp("-da", argv[i])) {
			pa->daddr = argv[i + 1];
			continue;
		}
		if (!strcmp("-sa", argv[i])) {
			pa->saddr = argv[i + 1];
			continue;
		}
		if (!strcmp("-dp", argv[i])) {
			pa->dport = argv[i + 1];
			continue;
		}
		if (!strcmp("-sp", argv[i])) {
			pa->sport = argv[i + 1];
			continue;
		}
		if (!strcmp("-id", argv[i])) {
			pa->ruleid = argv[i + 1];
			continue;
		}
	}

	if (!pa->iname && !pa->ruleid) {
		printf("No interceptor name nor rule id parsed.\n");
		return NULL;
	}

	return pa;
}
