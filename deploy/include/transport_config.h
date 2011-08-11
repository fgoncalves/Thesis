#ifndef __TRANSPORT_CONFIG_H__
#define __TRANSPORT_CONFIG_H__

extern uint8_t init_transport_config();
extern void free_transport_config();
extern void set_transport_parameter(char* transport_type, char* name,
		char* value);
extern char* get_transport_parameter(char* transport_type, char* name);
extern void dump_transport_configuration();

#endif
