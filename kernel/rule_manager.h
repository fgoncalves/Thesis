#ifndef __RULE_MANAGER_H__
#define __RULE_MANAGER_H__

#include "rule.h"
#include "klist.h"

extern int8_t start_rule_manager(void);
extern void stop_rule_manager(void);
extern void register_rule(rule* r);
extern void unregister_rule(uint32_t id);
extern void unregister_rules_for_interceptor(char* name);
extern klist_iterator* get_rules_iterator(void);
#endif
