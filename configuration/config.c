#include <string.h>
#include <stdio.h>
#include <regex.h>
#include <unistd.h>
#include <signal.h>
#include "macros.h"
#include "transport_config.h"
#include "configuration_parser.h"
#include "configuration_socket.h"

static char* config_file = "/etc/config_daemon.xml";

static void cleanup() {
	configuration_cleanup();
}

void print_usage(char* cmd) {
	printf("%s [-c <configuration_file>]\n", cmd);
}

void parse_args(char** args, int argc) {
	if (!(argc % 2)) {
		fprintf(stderr, "Invalid number of arguments");
		print_usage(args[0]);
		exit(-1);
	}

	int i;
	for (i = 1; i < argc;) {
		if (!strcmp(args[i], "-c")) {
			config_file = args[i + 1];
			i += 2;
			continue;
		}
	}
}

int main(int argc, char** argv) {
	if (argc > 1)
		parse_args(argv, argc);

	init_transport_config();
	parse_configuration_file(config_file);
	signal(SIGINT, cleanup);
	signal(SIGQUIT, dump_transport_configuration);

	serve();
	return 0;
}
