#ifndef __RULE_H__
#define __RULE_H__

#include "interceptor.h"
#include "filter.h"

typedef struct srule{
	uint32_t id;
	struct sint_desc* interceptor;
	uint32_t number_of_filters;
	struct sfilter** filters;
}rule;

extern rule* make_rule(int32_t nfilters);
extern void free_rule_and_unregister_filters(rule* r);
#endif
