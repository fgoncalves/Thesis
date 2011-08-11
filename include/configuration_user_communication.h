#ifndef __CONFIGURATION_USER_COMMUNICATION_H__
#define __CONFIGURATION_USER_COMMUNICATION_H__

#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>

typedef struct {
	int sockfd;
	struct sockaddr_un addr;
} config_descriptor;

extern config_descriptor* make_configuration_socket();
extern void free_configuration_socket(config_descriptor* fd);
extern char* com_get_transport_configuration_parameter(config_descriptor* fd,
		char* transport_type, char* parameter_name);
#endif
