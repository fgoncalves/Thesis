#ifndef __OPERATIONS_H__
#define __OPERATIONS_H__

#include <stdint.h>

extern int8_t write_to_proc(char op, char* interceptor_name, char* protocol,
		char* daddr, char* saddr, char* dport, char* sport, char* ruleid);
#endif
