#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>

#include "configuration_socket.h"
#include "config_msg.h"
#include "transport_config.h"
#include "macros.h"

#define CONFIGURATION_SOCKET_PATH "/tmp/configuration_socket"

static void send_msg(config_msg_t m, int sock_fd, struct sockaddr_un addr, int len){
	if(sendto(sock_fd, (char*) &m, sizeof(m), 0, (struct sockaddr*) &(addr), len) < 0){
		log(E, "Unable to send configuration message to client. Error: %s\n", strerror(errno));
	}
	return;
}

static void send_error(int sock_fd, struct sockaddr_un addr, int len){
	config_msg_t m;
	memset(&m, 0, sizeof(m));
	m.type = CONFIGURATION_ERROR;
	if(sendto(sock_fd, (char*) &m, sizeof(m), 0, (struct sockaddr*) &(addr), len) < 0){
		log(E, "Unable to send configuration message to client. Error: %s\n", strerror(errno));
	}
	return;
}

static void serve_request(config_msg_t m, int sock_fd, struct sockaddr_un addr,
		int len) {
	char* value;
	config_msg_t k;

	switch (m.type) {
	case GET_TRANSPORT_PARAMETER:
		value = get_transport_parameter(m.group, m.parameter);
		if (value) {
			debug(I, "Found value for %s:%s\n", m.group, m.parameter);
			memset(&k, 0, sizeof(config_msg_t));
			debug(I, "It is %s\n", value);
			strcpy(k.parameter, value);
			k.type = TRANSPORT_PARAMETER;
			send_msg(k, sock_fd, addr, len);
		} else{
			debug(W, "Could not find value for %s:%s\n", m.group, m.parameter);
			send_error(sock_fd, addr, len);
		}
		return;
	default:
		log(E, "Unknown request type: %d.\n", m.type);
		send_error(sock_fd, addr, len);
	}
	return;
}

void serve() {
	config_msg_t m;
	int server_sockfd, server_len, client_len = sizeof(struct sockaddr_un),
			status, on = 1;
	struct sockaddr_un server_address, client_address;

	unlink(CONFIGURATION_SOCKET_PATH);
	server_sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
	if (server_sockfd == -1) {
		log(E, "Unnale to create socket. Error: %s\n", strerror(errno));
		return;
	}

	memset(&server_address, 0, sizeof(server_address));
	memset(&client_address, 0, sizeof(client_address));
	server_address.sun_family = AF_LOCAL;
	strcpy(server_address.sun_path, CONFIGURATION_SOCKET_PATH);
	server_len = sizeof(server_address);
	bind(server_sockfd, (struct sockaddr*) &server_address, server_len);

	status
			= setsockopt(server_sockfd, SOL_SOCKET, SO_PASSCRED, &on,
					sizeof(on));
	if (status == -1) {
		log(E, "Setsockopt failed. Error: %s\n", strerror(errno));
		return;
	}

	while (true) {
		memset(&m, 0, sizeof(config_msg_t));
		status = recvfrom(server_sockfd, &m, sizeof(config_msg_t), 0,
				(struct sockaddr*) &client_address, &client_len);
		if (status < 0) {
			log(E, "Read returned and error. Error: %s\n", strerror(errno));
			continue;
		}
		serve_request(m, server_sockfd, client_address, client_len);
	}
}

