#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "configuration_user_communication.h"
#include "macros.h"
#include "config_msg.h"

char* com_get_transport_configuration_parameter(config_descriptor* fd,
		char* transport_type, char* parameter_name) {
	config_msg_t m;
	memset(&m, 0, sizeof(config_msg_t));
	m.type = GET_TRANSPORT_PARAMETER;
	strcpy(m.group, transport_type);
	strcpy(m.parameter, parameter_name);

	if (sendto(fd->sockfd, &m, sizeof(config_msg_t), 0,
			(struct sockaddr*) &(fd->addr), sizeof(struct sockaddr_un)) < 0) {
		log(E, "Unable to connect to configuration socket. Error: %s.\n", strerror(errno));
		return NULL;
	}

	int len = sizeof(struct sockaddr_un);

	memset(&m, 0, sizeof(config_msg_t));
	if (recvfrom(fd->sockfd, &m, sizeof(config_msg_t), 0,
			(struct sockaddr*) &(fd->addr), &len) < 0) {
		log(E, "Read returned and error. Error: %s\n", strerror(errno));
		return NULL;
	}

	switch (m.type) {
	case TRANSPORT_PARAMETER: {
		char* value = alloc(char, strlen(m.parameter));
		strcpy(value, m.parameter);
		return value;
	}
	case CONFIGURATION_ERROR:
		log(E, "Configuration error.\n");
		return NULL;
	}
	log(E, "Unable to determine message type.\n");
	return NULL;
}

config_descriptor* make_configuration_socket() {
	int len, status, on = 1;
	config_descriptor* fd = alloc(config_descriptor, 1);

	fd->sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);

	fd->addr.sun_family = AF_LOCAL;
	strcpy(fd->addr.sun_path, "/tmp/configuration_socket");
	len = sizeof(struct sockaddr_un);

	status = setsockopt(fd->sockfd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));
	if (status == -1) {
		log(E, "Setsockopt failed. Error: %s\n", strerror(errno));
		return NULL;
	}

	return fd;
}

void free_configuration_socket(config_descriptor* fd) {
	close(fd->sockfd);
	free(fd);
}
