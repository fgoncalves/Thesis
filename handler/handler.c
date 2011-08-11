#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include "handler.h"
#include "xml_parser.h"
#include "macros.h"
#include "reliable_transport.h"
#include "unreliable_transport.h"

static int8_t register_handler_with_registry_daemon(__tp(handler)* handler,
		struct sockaddr addr) {
	int status;
	if (!handler->flags.unix_unicast) {
		debug(W, "Handler has no unix address. There's no need to register it.\n");
		return 0;
	}debug(I, "Registering handler with daemon.\n");

	if ((status = register_handler(handler->id, addr,
			handler->module_communication.regd)) < 0) {
		log(E, "Unable to register handler with daemon. Error: %s\n",
				registry_error_str(-status));
		return 0;
	}
	return 1;
}

static int8_t setup_unix_unicast(__tp(handler)* handler, char* unix_path) {
	int server_len, status, on = 1;
	struct sockaddr_un server_address;

	debug(I, "Setting unix on handler.\n");
	if (strlen(unix_path) > 13) {
		log(
				E,
				"This implementation only supports unix paths with less than 13 characters.\n");
		return 0;
	}

	unlink(unix_path);

	handler->sockets.unix_fd = socket(AF_LOCAL, SOCK_DGRAM, 0);

	server_address.sun_family = AF_LOCAL;
	strcpy(server_address.sun_path, unix_path);
	server_len = sizeof(server_address);
	bind(handler->sockets.unix_fd, (struct sockaddr*) &server_address,
			server_len);

	status = setsockopt(handler->sockets.unix_fd, SOL_SOCKET, SO_PASSCRED, &on,
			sizeof(on));
	if (status == -1) {
		log(E, "Unnable to set socket option (SO_PASSCRED). Error: %s\n",
				strerror(errno));
		return 0;
	}

	handler->flags.unix_unicast = 1;
	if (!register_handler_with_registry_daemon(handler,
			*(struct sockaddr*) &server_address))
		return 0;

	return 1;
}

static int8_t setup_unicast(__tp(handler)* handler, char* unicast_ip,
		uint16_t port) {
	int broadcast = 1;

	debug(I, "Setting unicast on handler.\n");
	//sanity check
	if (handler->flags.broadcast) {
		close(handler->sockets.inet_fd);
		handler->flags.broadcast = 0;
	}

	handler->sockets.inet_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (handler->sockets.inet_fd == -1) {
		log(E, "Unable to open socket for handler. Error: %s.\n",
				strerror(errno));
		close(handler->sockets.inet_fd);
		return 0;
	}

	if ((setsockopt(handler->sockets.inet_fd, SOL_SOCKET, SO_BROADCAST,
			&broadcast, sizeof broadcast)) == -1) {
		perror("setsockopt failed:");
		close(handler->sockets.inet_fd);
		return 1;
	}

	struct sockaddr_in unicast_addr;
	unicast_addr.sin_family = AF_INET;
	unicast_addr.sin_port = htons(port);
	inet_aton(unicast_ip, &unicast_addr.sin_addr);

	if (bind(handler->sockets.inet_fd, (struct sockaddr *) &unicast_addr,
			sizeof(unicast_addr)) == -1) {
		log(
				E,
				"Cannot create socket for handler. Unnable to bind socket. Error: %s\n",
				strerror(errno));
		close(handler->sockets.inet_fd);
		return 0;
	}

	handler->flags.unicast = 1;
	return 1;
}

static int8_t setup_broadcast(__tp(handler)* handler, uint16_t port) {
	int broadcast = 1;

	debug(I, "Setting broadcast on handler.\n");
	//sanity check
	if (handler->flags.unicast) {
		close(handler->sockets.inet_fd);
		handler->flags.unicast = 0;
	}

	handler->sockets.inet_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (handler->sockets.inet_fd == -1) {
		log(E, "Unable to open socket for handler. Error: %s.\n",
				strerror(errno));
		close(handler->sockets.inet_fd);
		return 0;
	}

	if ((setsockopt(handler->sockets.inet_fd, SOL_SOCKET, SO_BROADCAST,
			&broadcast, sizeof broadcast)) == -1) {
		perror("setsockopt failed:");
		close(handler->sockets.inet_fd);
		return 1;
	}

	struct sockaddr_in broadcast_addr;
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_port = htons(port);
	broadcast_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(handler->sockets.inet_fd, (struct sockaddr *) &broadcast_addr,
			sizeof(broadcast_addr)) == -1) {
		log(
				E,
				"Cannot create socket for handler. Unnable to bind socket. Error: %s\n",
				strerror(errno));
		close(handler->sockets.inet_fd);
		return 0;
	}

	handler->flags.broadcast = 1;
	return 1;
}

static void setup_active_fds(__tp(handler) * handler) {
	FD_ZERO(&(handler->sockets.active_fds));
	if (handler->flags.unicast || handler->flags.broadcast)
		FD_SET(handler->sockets.inet_fd, &(handler->sockets.active_fds));
	if (handler->flags.unix_unicast)
		FD_SET(handler->sockets.unix_fd, &(handler->sockets.active_fds));
}

static int8_t determine_address_family(char *type, char *addr, char *port,
		char *enable_broadcast, __tp(handler)* handler) {
	if (!type) {
		log(E, "Unnable to determine handler address type.");
		return 0;
	}

	if (!strcmp(type, "inet")) {
		if (!addr || !port) {
			log(
					E,
					"Inet address type requires an ip address and a port number.\n");
			return 0;
		}
		if (!setup_unicast(handler, addr, atoi(port)))
			return 0;
		if (enable_broadcast)
			if (!setup_broadcast(handler, atoi(port)))
				return 0;
		return 1;
	}

	if (!strcmp(type, "unix")) {
		if (!addr) {
			log(E, "Unix address type requires a unix path to a file.\n");
			return 0;
		}
		if (!setup_unix_unicast(handler, addr))
			return 0;
		return 1;
	}

	log(E, "Unnable to determine handler address type.");
	return 0;
}

static int8_t handler_start_element_cb(char* tag, attr_list* l, void* user_data) {
	__tp(handler)*handler = (__tp(handler)*) user_data;
	if (!strcmp(tag, "handler")) {
		attr* at = l->first;
		int8_t found_id = 0;
		for (; at != NULL; at = at->next)
			if (!strcmp(at->name, "id")) {
				found_id = 1;
				handler->id = atoi(at->value);
			}
		if (!found_id) {
			log(E, "Could not find handler id.\n");
			return 0;
		}
		handler->module_communication.regd = make_registry_socket();
		if (!handler->module_communication.regd) {
			log(
					E,
					"Check if registry daemon is running. Could not make registor descriptor.\n");
			return 0;
		}
		return 1;
	}
	if (!strcmp(tag, "transport")) {
		attr* at = l->first;
		int8_t found_type = 0;
		for (; at != NULL; at = at->next)
			if (!strcmp(at->name, "type")) {
				found_type = 1;
				if (!strcmp(at->value, "reliable"))
					handler->transport.create_transport_cb =
							__tp(create_reliable_transport);
				if (!strcmp(at->value, "unreliable"))
					handler->transport.create_transport_cb =
							__tp(create_unreliable_transport);
			}
		if (!found_type) {
			log(E, "Could not find handler transport type.\n");
			return 0;
		}
		return 1;
	}
	if (!strcmp(tag, "address")) {
		attr* at = l->first;
		char *type = NULL, *addr = NULL, *port = NULL, *enable_broadcast = NULL;
		for (; at != NULL; at = at->next) {
			if (!strcmp(at->name, "type"))
				type = at->value;
			if (!strcmp(at->name, "addr"))
				addr = at->value;
			if (!strcmp(at->name, "port"))
				port = at->value;
			if (!strcmp(at->name, "enable_broadcast")
					&& !strcmp(at->value, "true"))
				enable_broadcast = at->value;
		}
		return determine_address_family(type, addr, port, enable_broadcast,
				handler);
	}

	return 1;
}

static int8_t handler_end_element_cb(char* tag, void* user_data) {
	return 1;
}

static int8_t handler_text_block_element_cb(char* text, uint32_t len,
		void* user_data) {
	return 1;
}

//### Cleanup memory used by handler. Should only be called if handler was created successfuly. ###
//---
//   + handler - Transport handler to be freed. This pointer will point to null after this call.
void __tp(free_handler)(__tp(handler)* sk) {
	if (!sk)
		return;

	if (pthread_cancel(sk->threads.receiver))
		log(W, "Handler could not cancel receiver.\n");

	if (pthread_cancel(sk->threads.sender))
		log(W, "Handler could not cancel sender.\n");

	__tp(destroy_buffer)(sk->outgoing_messages);

	if (sk->flags.unicast || sk->flags.broadcast)
		close(sk->sockets.inet_fd);
	if (sk->flags.unix_unicast) {
		close(sk->sockets.unix_fd);
		unregister_handler_address(sk->id, sk->module_communication.regd);
		free_registry_socket(sk->module_communication.regd);
	}

	free_configuration_socket(sk->module_communication.confd);

	free(sk);
	sk = NULL;
	return;
}

//###  Build a handler structure based on an XML file. ###
//---
//   + filename - XML handler configuration file.
//   + receive_cb - Callback function created by application.
//> **Return**: An address point to a transport_layer_handler, or **NULL** in case of an error.
__tp(handler)* __tp(create_handler_based_on_file)(
		char* filename,
		void(*receive_cb)(struct __tp(app_handler)*, char*, uint16_t, int64_t,
				int64_t, uint16_t)) {
	parser_cbs cbs;
	__tp(handler)* handler = alloc(__tp(handler), 1);

	handler->module_communication.confd = make_configuration_socket();

	handler->application.receive_cb = receive_cb;

	cbs.user_data = handler;
	cbs.start = handler_start_element_cb;
	cbs.end = handler_end_element_cb;
	cbs.text = handler_text_block_element_cb;
	parse_xml(cbs, filename);

	//This will ensure that sockiface gets the right maxfd for select call
	if (!handler->flags.unicast && !handler->flags.broadcast)
		handler->sockets.inet_fd = -1;
	if (!handler->flags.unix_unicast)
		handler->sockets.unix_fd = -1;

	setup_active_fds(handler);

	handler->transport.create_transport_cb(handler);
	return handler;
}

//###  Build a handler structure based on an XML string. ###
//---
//   + xml_string - XML handler configuration string.
//   + receive_cb - Callback function created by application.
//> **Return**: An address point to a transport_layer_handler, or **NULL** in case of an error.
__tp(handler)* __tp(create_handler_based_on_string)(
		char* xml_string,
		void(*receive_cb)(struct __tp(app_handler)*, char*, uint16_t, int64_t,
				int64_t, uint16_t)) {
	parser_cbs cbs;
	__tp(handler)* handler = alloc(__tp(handler), 1);
	handler->outgoing_messages = __tp(new_message_buffer)(
			DEFAULT_TRANSPORT_OUTGOING_MESSAGE_BUFFER_SIZE);

	if (!handler->outgoing_messages) {
		log(E, "Failed to initialize outgoing message buffer.\n");
		free(handler);
		return NULL;
	}

	handler->application.receive_cb = receive_cb;

	cbs.user_data = handler;
	cbs.start = handler_start_element_cb;
	cbs.end = handler_end_element_cb;
	cbs.text = handler_text_block_element_cb;
	parse_xml_string(cbs, xml_string);

	//This will ensure that sockiface gets the right maxfd for select call
	if (!handler->flags.unicast && !handler->flags.broadcast)
		handler->sockets.inet_fd = -1;
	if (!handler->flags.unix_unicast)
		handler->sockets.unix_fd = -1;

	setup_active_fds(handler);

	handler->transport.create_transport_cb(handler);
	return handler;
}
