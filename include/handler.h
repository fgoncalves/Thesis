#ifndef __HANDLER_H__
#define __HANDLER_H__

#include <stdint.h>
#include <sys/socket.h>
#include <pthread.h>
#include "transport.h"
#include "message_buffer.h"
#include "macros.h"
#include "registry_error.h"
#include "registry_ops.h"
#include "configuration_user_communication.h"

#define DEFAULT_TRANSPORT_OUTGOING_MESSAGE_BUFFER_SIZE 20
#define DEFAULT_RELIABLE_TRANSPORT_WINDOW_SIZE 5
#define DEFAULT_RELIABLE_TRANSPORT_RECEIVE_BUFFER_SIZE 5
#define DEFAULT_RELIABLE_TRANSPORT_SACK_TIMEOUT 2 //seconds

typedef struct __tp(app_handler) {
	uint16_t id;
	__tp(buffer)* outgoing_messages;

	struct {
		int inet_fd;
		int unix_fd;
		fd_set active_fds;
	} sockets;

	struct {
		pthread_t receiver;
		pthread_t sender;
	} threads;

	struct { //Callbacks
		void (*create_transport_cb)(struct __tp(app_handler)* sk);
		void (*receive_cb)(struct __tp(app_handler)* sk, __tp(pdu)* pdu);
		void* control_data;
	} transport;

	struct {
		void (*receive_cb)(struct __tp(app_handler)* sk, char* data,
				uint16_t len, int64_t timestamp, int64_t air_time, uint16_t src_id);
		int8_t (*send_cb)(struct __tp(app_handler)* sk, char* data,
				uint16_t len, uint16_t dst_id);
		int8_t (*send_with_timestamp_cb)(struct __tp(app_handler)* sk,
				char* data, uint16_t len, uint16_t dst_id, int64_t ts);
		int8_t (*close_handler_cb)(struct __tp(app_handler)* sk);
	} application;

	struct {
		registry_descriptor* regd;
		config_descriptor* confd;
	} module_communication;

	struct {
		uint8_t unicast :1, broadcast :1, unix_unicast :1, reserved :5;
	} flags;
}__tp(handler);

extern __tp(handler)* __tp(create_handler_based_on_file)( char* filename, void (*receive_cb)(struct __tp(app_handler)*, char*, uint16_t, int64_t, int64_t, uint16_t));
extern __tp(handler)* __tp(create_handler_based_on_string)( char* xml_string, void (*receive_cb)(struct __tp(app_handler)*, char*, uint16_t, int64_t, int64_t, uint16_t));
extern void __tp(free_handler)(__tp(handler)* sk);

/*Applications should use these macros to invoke each function.*/
#define send_data(handler, data, data_len, dst_id)	\
  (handler)->application.send_cb((handler), (data), (data_len), (dst_id))

#define send_data_with_ts(handler, data, data_len, ts, dst_id)	\
  (handler)->application.send_with_timestamp_cb((handler), (data), (data_len), (dst_id), (ts))

#define close_transport_handler(handler)				\
  do{									\
    if((handler)->application.close_handler_cb)					\
      (handler)->application.close_handler_cb((handler));				\
  }while(0)

#endif
