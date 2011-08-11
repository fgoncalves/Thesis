/*
 * Before reading this file, I recommend reading rule.c.
 *
 * This file manages interceptors. It is responsible for registering and unregistering them.
 *
 * It is through here that the framework will ask interceptors to create the appropriate rules.
 * */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/in.h>

#include "interceptor.h"
#include "interceptor_manager.h"
#include "rule_manager.h"
#include "klist.h"
#include "proc_entry.h"

/*******************************************************
 * First define a linked list to hold each interceptor *
 *******************************************************/

static klist_node* new_interceptor_list_node(
		interceptor_descriptor* interceptor) {
	klist_node* i = make_klist_node(interceptor);
	return i;
}

static klist* new_interceptor_list(void) {
	return make_klist();
}

static void add_interceptor_to_list(klist* l,
		interceptor_descriptor* interceptor) {
	klist_node *n = new_interceptor_list_node(interceptor);
	add_klist_node_to_klist(l, n);
}

static void remove_interceptor_from_list(klist* list, klist_node* interceptor) {
	remove_klist_node_from_klist(list, interceptor);
}

static void free_interceptor_list(klist* l) {
	free_klist(l, NULL);
}
/**************************************Interceptor List End***********************/

//List of registered interceptors
klist* int_list = NULL;

//Called by the framework to start this manager
int8_t start_interceptor_manager(void) {
	int_list = new_interceptor_list();
	if (!int_list)
		return 0;
	return 1;
}

//Called by the framework to stop this manager
void stop_interceptor_manager(void) {
	if (int_list)
		free_interceptor_list(int_list);
}

/* This function is exported. It should be called in the init_module
 * function of each interceptor*/
int8_t register_interceptor(interceptor_descriptor* i) {
	if (int_list) {
		add_interceptor_to_list(int_list, i);
		mess_proc_entry();
		return 1;
	}
	return 0;
}

/* This function is exported. It should be called in the cleanup_module
 * function of each interceptor*/
int8_t unregister_interceptor(char* name) {
	klist_iterator* i = NULL;
	klist_node* kn = NULL;
	interceptor_descriptor *id = NULL;
	if (!int_list)
		return 0;

	//Find interceptor
	for (i = make_klist_iterator(int_list); klist_iterator_has_next(i);) {
		kn = klist_iterator_next(i);
		id = (interceptor_descriptor*) kn->data;
		if (!strcmp(name, id->name))
			break;
	}
	if (!kn)
		return 0;

	//Clean interceptor and registered rules.
	unregister_rules_for_interceptor(name);
	remove_interceptor_from_list(int_list, kn);
	free_klist_iterator(i);
	mess_proc_entry();
	return 1;
}

/*
 * Obtain an iterator to the interceptor list maintained by this manager.
 * Although it is possible to destroy such list, I recommend not doing it.
 * */
klist_iterator* get_interceptor_iterator(void) {
	return make_klist_iterator(int_list);
}

/*
 * This function will "ask" the interceptor registered with the name 'interceptor_name'
 * to create a rule for the given filter specification. It will then register such rule
 * with the rules manager.
 * */
uint8_t create_rule(char* interceptor_name, filter_specs* sp) {
	klist_iterator* i = NULL;
	interceptor_descriptor* id;
	rule* r = NULL;
	if (!int_list)
		return 0;

	for (i = make_klist_iterator(int_list); klist_iterator_has_next(i);) {
		id = (interceptor_descriptor*) (klist_iterator_next(i))->data;
		if (!strcmp(interceptor_name, id->name)) {
			if ((r = id->create_rule_for_specs_cb(sp)) == NULL){
				free_klist_iterator(i);
				return 0;
			}
			register_rule(r);
			free_klist_iterator(i);
			return 1;
		}
	}
	free_klist_iterator(i);
	return 0;
}

/*
 * When a rule is registered an id will be assigned to it. This can be viewed in the
 * proc entry, which lists interceptors and rules. Such id can be passed to this function
 * in order to remove the corresponding rule.
 * */
void remove_rule(uint32_t rule_id){
	unregister_rule(rule_id);
}

EXPORT_SYMBOL( register_interceptor);
EXPORT_SYMBOL( unregister_interceptor);

