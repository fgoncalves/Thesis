/*
 * This file is just an header file, which should be included by each interceptor.
 *
 * Each interceptor will need to create an interceptor_descriptor structure and
 * call register_interceptor in order to register itself with the framework.
 * */

#ifndef __INTERCEPTOR_H__
#define __INTERCEPTOR_H__

#include "filter.h"
#include "rule.h"

struct sfspecs;

typedef struct sint_desc{
	char* name;
	struct srule* (*create_rule_for_specs_cb)(struct sfspecs*);
}interceptor_descriptor;

extern int8_t register_interceptor(interceptor_descriptor* i);
extern int8_t unregister_interceptor(char* name);
#endif
