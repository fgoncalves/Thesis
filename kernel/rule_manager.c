/*
 * Before reading this file I recommend reading klist.c.
 *
 * This file implements a rule manager. This manager is responsible for registering and unregistering
 * rules. It communicates only with the interceptor manager, thus all interaction with this
 * manager is done through the interceptor manager.
 * */

#include <linux/kernel.h>
#include <linux/module.h>
#include "klist.h"
#include "rule.h"
#include "rule_manager.h"
#include "chains.h"
#include "proc_entry.h"

klist* manager_rule_list = NULL;

int8_t start_rule_manager(void) {
	manager_rule_list = make_klist();
	if (!manager_rule_list)
		return 0;
	return 1;
}

static void free_rule_cb(void* r) {
	free_rule_and_unregister_filters((rule*) r);
}

void stop_rule_manager(void) {
	free_klist(manager_rule_list, free_rule_cb);
}

void register_rule(rule* r) {
	klist_node* kn;
	int32_t i;
	filter* f;
	if (manager_rule_list) {
		kn = make_klist_node(r);
		if (kn) {
			add_klist_node_to_klist(manager_rule_list, kn);
			for (i = 0; i < r->number_of_filters; i++) {
				f = (r->filters)[i];
				if (!register_filter(f))
					printk("** Unable to register filter\n");
			}
			mess_proc_entry();
		}
	}
}

void unregister_rules_for_interceptor(char* name) {
	klist_iterator* it;
	rule* r;
	for (it = make_klist_iterator(manager_rule_list); klist_iterator_has_next(
			it);) {
		r = (rule*) (klist_iterator_peek(it))->data;
		if (!strcmp(name, r->interceptor->name)) {
			r = (rule*) klist_iterator_remove_current(it);
			if (r)
				free_rule_and_unregister_filters(r);
			mess_proc_entry();
		}
	}
}

klist_iterator* get_rules_iterator(void) {
	return make_klist_iterator(manager_rule_list);
}

void unregister_rule(uint32_t id) {
	klist_iterator* it;
	rule* r;
	it = make_klist_iterator(manager_rule_list);
	while (klist_iterator_has_next(it)) {
		r = (rule*) (klist_iterator_peek(it))->data;
		if (r->id == id) {
			r = (rule*) klist_iterator_remove_current(it);
			if (r) {
				free_rule_and_unregister_filters(r);
			}
			mess_proc_entry();
			free_klist_iterator(it);
			return;
		}
		klist_iterator_next(it);
	}
	free_klist_iterator(it);
}
