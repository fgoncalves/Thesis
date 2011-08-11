#ifndef __INTERCEPTOR_MANAGER_H__
#define __INTERCEPTOR_MANAGER_H__

#include "filter.h"
#include "klist.h"

extern int8_t start_interceptor_manager(void);
extern void stop_interceptor_manager(void);
extern klist_iterator* get_interceptor_iterator(void);
extern uint8_t create_rule(char* interceptor_name, filter_specs* sp);
extern void remove_rule(uint32_t rule_id);
#endif
