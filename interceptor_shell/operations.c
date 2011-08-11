#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "operations.h"
#include "macros.h"

#define PROC_FILE "/proc/interceptor_registry"

static void itoa(int num, char *str) {
	sprintf(str, "%d", num);
}

int8_t write_to_proc(char op, char* interceptor_name, char* protocol,
		char* daddr, char* saddr, char* dport, char* sport, char* ruleid) {
	int proc_fd, buff_offset = 0;
	char buffer[800] = { 0 }, helper[30] = { 0 };
	struct in_addr addr;
	int32_t dip = 0, sip = 0;
	int16_t dp = 0, sp = 0;
	uint8_t proto = 0;

	buffer[0] = op;
	buffer[1] = ':';
	buff_offset = 2;

	switch (op) {
	case 'R':
		memcpy(buffer + buff_offset, interceptor_name, strlen(interceptor_name));
		buff_offset += strlen(interceptor_name);
		buffer[buff_offset++] = ':';

		if (protocol) {
			if (!strcmp("UDP", protocol)) {
				itoa(IPPROTO_UDP, helper);
				memcpy(buffer + buff_offset, helper, strlen(helper));
				buff_offset += strlen(helper);
				buffer[buff_offset++] = ':';
				memset(&helper, 0, sizeof(helper));
			}
		} else
			buffer[buff_offset++] = ':';

		if (daddr) {
			memset(&addr, 0, sizeof(addr));
			if (inet_pton(AF_INET, daddr, &addr) < 0) {
				printf("Could not convert address %s\n", daddr);
				return -1;
			}
			dip = addr.s_addr;
			itoa(dip, helper);
			memcpy(buffer + buff_offset, helper, strlen(helper));
			buff_offset += strlen(helper);
			buffer[buff_offset++] = ':';
			memset(&helper, 0, sizeof(helper));
		} else
			buffer[buff_offset++] = ':';

		if (saddr) {
			memset(&addr, 0, sizeof(addr));
			if (inet_pton(AF_INET, saddr, &addr) < 0) {
				printf("Could not convert address %s\n", saddr);
				return -1;
			}
			sip = addr.s_addr;
			itoa(sip, helper);
			memcpy(buffer + buff_offset, helper, strlen(helper));
			buff_offset += strlen(helper);
			buffer[buff_offset++] = ':';
			memset(&helper, 0, sizeof(helper));
		} else
			buffer[buff_offset++] = ':';

		if (dport) {
			itoa(htons(atoi(dport)), helper);
			memcpy(buffer + buff_offset, helper, strlen(helper));
			buff_offset += strlen(helper);
			buffer[buff_offset++] = ':';
			memset(&helper, 0, sizeof(helper));
		} else
			buffer[buff_offset++] = ':';
		
		if (sport) {
			itoa(htons(atoi(sport)), helper);
			memcpy(buffer + buff_offset, helper, strlen(helper));
			buff_offset += strlen(helper);
			memset(&helper, 0, sizeof(helper));
		}

		debug(I, "Writting '%s' to proc.\n", buffer);

		proc_fd = open(PROC_FILE, O_WRONLY);
		if (proc_fd == -1) {
		  perror("Unable to open proc file");
		  return 0;
		}

		if (write(proc_fd, buffer, sizeof(buffer)) == -1) {
			perror("Unable to write command to proc file");
			return 0;
		}

		return 1;
	case 'U':
		if (!ruleid) {
			printf("Unable to parse rule id.\n");
			return 0;
		}

		itoa(atoi(ruleid), helper);
		memcpy(buffer + buff_offset, helper, strlen(helper));
		buff_offset += strlen(helper);
		memset(&helper, 0, sizeof(helper));
		debug(I, "Writting '%s' to proc.\n", buffer);

		proc_fd = open(PROC_FILE, O_WRONLY);
		if (proc_fd == -1) {
		  perror("Unable to open proc file");
		  return 0;
		}

		if (write(proc_fd, buffer, sizeof(buffer)) == -1) {
			perror("Unable to write command to proc file");
			return 0;
		}
		return 1;
	default:
		printf("Unknown command %c\n", op);
	}
	return 0;
}

