#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fred/macros.h>
#include "addr_convert.h"
#include "argparse.h"
#include "cmd.h"

char* upper_case(char* str) {
	int i = 0;
	for (; str[i] != '\0'; i++)
		if (str[i] >= 'a' && str[i] <= 'z')
			str[i] -= ('a' - 'A');
	return str;
}

uint8_t parse_args_and_execute(char **argv, int argc) {
	struct sockaddr_in* in;
	struct sockaddr_un* un;
	uint8_t res;
	command* c;
	struct sockaddr_in sock;
	struct sockaddr_un unix_sock;
	uint16_t hid;

	if (argc >= 2) {
		if (strcmp(upper_case(argv[1]), "G") == 0) {
			if (argc >= 3) {
				hid = dot2int(atoi(argv[2]) / 1000, atoi(argv[2]) % 1000);
				command* c = create_command(CMD_GET);
				if (!c)
					return 1;

				struct sockaddr* s = c->cmd.get_handler_address_cb(c, hid);
				free_command(c);
				if (!s) {
					printf("Cannot find address for handler %s\n", argv[2]);
					return 1;
				}

				switch (s->sa_family) {
				case AF_INET:
					in = (struct sockaddr_in*) s;
					printf("%s:%u\n", inet_ntoa(in->sin_addr),
							ntohs(in->sin_port));
					break;
				case AF_LOCAL:
					un = (struct sockaddr_un*) s;
					printf("%s\n", un->sun_path);
					break;
				default:
					printf("Got address but its family is not supported.\n");
				}
				return 1;
			} else
				return 0;
		}

		if (strcmp(upper_case(argv[1]), "U") == 0) {
			if (argc >= 3) {
				hid = dot2int(atoi(argv[2]) / 1000, atoi(argv[2]) % 1000);
				c = create_command(CMD_UNREGISTER);
				if (!c)
					return 1;
				res = c->cmd.unregister_handler_address_cb(c, hid);
				free_command(c);
				return res;
			} else
				return 0;
		}

		if (strcmp(upper_case(argv[1]), "R") == 0) {
			hid = dot2int(atoi(argv[2]) / 1000, atoi(argv[2]) % 1000);
			c = create_command(CMD_REGISTER);
			if (!c) {
				debug(W, "Failed to create command\n");
				return 1;
			}
			switch (argc) {
			case 4: //unix family
				if(strlen(argv[3]) > 13){
					log(E, "This implementation only supports unix paths up to 13 characters.\n");
					return 0;
				}
				memset(&unix_sock, 0, sizeof(unix_sock));
				unix_sock.sun_family = AF_LOCAL;
				strcpy(unix_sock.sun_path, argv[3]);

				res = c->cmd.register_handler_address_cb(c, hid,
						*((struct sockaddr*) &unix_sock));
				free_command(c);
				return res;
			case 5: //inet family
				memset(&sock, 0, sizeof(struct sockaddr));
				sock.sin_family = AF_INET;
				sock.sin_port = htons(atoi(argv[4]));
				sock.sin_addr.s_addr = inet_addr(argv[3]);
				if (sock.sin_addr.s_addr == (in_addr_t) (-1)) {
					printf("Unable to parse ip %s.\n", argv[2]);
					return 0;
				}
				res = c->cmd.register_handler_address_cb(c, hid,
						*((struct sockaddr*) &sock));
				free_command(c);
				return res;
			default:
				free_command(c);
				return 0;
			}
		}
	}

	return 0;
}
