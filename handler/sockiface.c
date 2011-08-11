#include <errno.h>
#include <arpa/inet.h>
#include "sockiface.h"
#include "macros.h"

int8_t sockiface(send_pdu)(__tp(handler)* h, __tp(pdu)* p, struct sockaddr dst) {
	int status, fd;
	int32_t len;
	char *to_send = NULL;

	len = sizeof(control_byte) + sizeof(__tp(pdu)) + p->len;
	debug(I, "Sending %d bytes.\n", len);
	to_send = alloc_bytes(char, len);
	swap_pdu_byte_order(p);
	memcpy(to_send + sizeof(control_byte), p, len - sizeof(control_byte));
	swap_pdu_byte_order(p); //Put pdu like it was

	switch (dst.sa_family) {
	case AF_LOCAL:
		debug(I, "Destination address is a UNIX path.\n");
		fd = h->sockets.unix_fd;
		break;
	case AF_INET:
		debug(I, "Destination address is a INET path.\n");
		fd = h->sockets.inet_fd;
		break;
	default:
		log(E, "Unsupported address family.\n");
		return 0;
	}

	debug(I, "Using descriptor %d to send data.\n", fd);

#ifdef DEBUG
	switch (dst.sa_family) {
		case AF_LOCAL:
		debug(I, "Sending data to %s\n", ((struct sockaddr_un*) &dst)->sun_path);
		break;
		case AF_INET:{
			int i;
		    printf("sendto(%d, [", fd);
		    for(i = 0; i < len; i++)
		    	if((to_send[i] < 'z' && to_send[i] > 'a') || (to_send[i] < 'Z' && to_send[i] > 'A'))
		    		printf("%c", to_send[i]);
		    	else
		    		printf(".");
		    printf("], %d, 0, %s:%u, %d);\n", len, inet_ntoa(((struct sockaddr_in*) &dst)->sin_addr), ntohs(((struct sockaddr_in*) &dst)->sin_port), sizeof(struct sockaddr));
		}
		break;
	}
#endif

	debug(I, "Sending pdu %d\n", p->seq);
	status = sendto(fd, to_send, len, 0, &dst, sizeof(struct sockaddr));
	free(to_send);
	if (status == -1) {
		log(E, "Sendto failed. Error: %s\n", strerror(errno));
		return 0;
	}debug(I, "Actually sent %d bytes.\n", status);

#ifdef DEBUG
	switch (dst.sa_family) {
		case AF_LOCAL:
		debug(I, "Sent data to %s\n", ((struct sockaddr_un*) &dst)->sun_path);
		break;
		case AF_INET:
		debug(I, "Sent data to %s:%u\n", inet_ntoa(((struct sockaddr_in*) &dst)->sin_addr), ntohs(((struct sockaddr_in*) &dst)->sin_port));
		break;
	}
#endif
	return 1;
}

void sockiface(receive_pdu)(__tp(handler)* handler) {
	struct sockaddr client_addr;
	int cl_len, status = 0;
	int
			max_fd =
					(handler->sockets.unix_fd > handler->sockets.inet_fd) ? handler->sockets.unix_fd
							: handler->sockets.inet_fd;
	//Sanity check!!
	if (max_fd == -1) {
		log(E, "Handler is not has no valid socket descriptor. Thus, I'm in panic and returning.\n");
		return;
	}

	fd_set read_fds;
	char
			*pdu_and_cb =
					alloc_bytes(char, sizeof(control_byte) + sizeof(__tp(pdu)) + PDU_MAX_PAYLOAD_SIZE);
	while (true) {
		memset(pdu_and_cb, 0,
				sizeof(control_byte) + sizeof(__tp(pdu)) + PDU_MAX_PAYLOAD_SIZE);
		read_fds = handler->sockets.active_fds;
		debug(I, "Listening on handler socket fds [%d:%d]...\n", handler->sockets.inet_fd, handler->sockets.unix_fd);
		if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
			log(E, "Select failed. Error: %s\n", strerror(errno));
			return;
		}debug(I, "Got something in a socket descriptor. Going to receive it...\n");
		memset(pdu_and_cb, 0,
				sizeof(control_byte) + sizeof(__tp(pdu)) + PDU_MAX_PAYLOAD_SIZE);
		cl_len = sizeof(client_addr);
		if (handler->sockets.inet_fd > 0
				&& FD_ISSET(handler->sockets.inet_fd, &read_fds)) {
			status = recvfrom(
					handler->sockets.inet_fd,
					pdu_and_cb,
					sizeof(__tp(pdu)) + PDU_MAX_PAYLOAD_SIZE
							+ sizeof(control_byte), 0, &client_addr, &cl_len);
		}
		if (handler->sockets.unix_fd > 0
				&& FD_ISSET(handler->sockets.unix_fd, &read_fds)) {
			status = recvfrom(
					handler->sockets.unix_fd,
					pdu_and_cb,
					sizeof(__tp(pdu)) + PDU_MAX_PAYLOAD_SIZE
							+ sizeof(control_byte), 0, &client_addr, &cl_len);
		}
		if (status == -1) {
			log(E, "Recvfrom returned -1. Error: %s\n", strerror(errno));
			continue;
		}
		if (handler->transport.receive_cb) {
			debug(I, "Calling transport callback...\n");
			__tp(pdu)* pdu = (__tp(pdu)*) (pdu_and_cb + sizeof(control_byte));
			__tp(pdu)* cp_pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)) + pdu->len);
			memcpy(cp_pdu, pdu, sizeof(__tp(pdu)) + pdu->len);
			swap_pdu_byte_order(cp_pdu);
			debug(I, "Received pdu %d\n", cp_pdu->seq);
			handler->transport.receive_cb(handler, cp_pdu);
			debug(I, "Transport callback ended.\n");
		}
	}
	return;
}
