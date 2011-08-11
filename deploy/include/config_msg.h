#ifndef __CONFIG_MSG_H__
#define __CONFIG_MSG_H__

#include <stdint.h>

#define MAX_FIELD_LENGTH 256

enum{
	CONFIGURATION_ERROR,
	GET_TRANSPORT_PARAMETER,
	TRANSPORT_PARAMETER,
};

typedef struct{
	uint8_t type;
	char group[MAX_FIELD_LENGTH];
	char parameter[MAX_FIELD_LENGTH];
}config_msg_t;

#endif

