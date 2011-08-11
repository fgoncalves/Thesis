/*
 * Before reading this file I recommend reading filter.c.
 *
 * This file implements the rule structure and all related functions.
 *
 * A rule is simply a set of filters that an interceptor created and registered with the
 * rule manager. A rule can be viewed as an abstraction to traffic interception. Instead of
 * specifying which filters should run at which entry points, a rule is created and filters
 * will be automatically generated for it.
 *
 * For example, synchronization requires 2 filters. One at PREROUTING and another at POSTROUTING.
 * Suppose a node needs to synchronize traffic sent from it to other nodes on a specific UDP port.
 * If filters were the only structure available, one would have to know which ones to apply and keep track of them.
 * Having a rule abstraction, one can simply create a rule to synchronize traffic at said UDP port and
 * filters will automatically be generated and applied.
 * */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "rule.h"
#include "chains.h"

/*This function will free the filters specification. Please note that this specification is the same for all filters in the rule.*/
void free_rule_and_unregister_filters(rule* r) {
	int32_t i;
	if (!r)
		return;
	for (i = 0; i < r->number_of_filters; free_filter((r->filters)[i++])){
		unregister_filter((r->filters)[i]);
		if (i == 0)
			free_filter_specs(((r->filters)[0])->filter_specs);
	}
	kfree(r);
}

/*Create a rule with space for 'nfilters'*/
rule* make_rule(int32_t nfilters) {
	static uint32_t id = 0;
	rule* r = kmalloc(sizeof(rule), GFP_ATOMIC);
	if (!r) {
		printk("%s in %s:%d: kmalloc failed. Unable to create rule.\n",
				__FILE__, __FUNCTION__, __LINE__);
		return NULL;
	}
	memset(r, 0, sizeof(rule));
	r->filters = kmalloc(sizeof(filter*) * nfilters, GFP_ATOMIC);
	if (!r->filters) {
		printk("%s in %s:%d: kmalloc failed. Unable to allocate filters for rule.\n",
				__FILE__, __FUNCTION__, __LINE__);
		return NULL;
	}
	memset(r->filters, 0, sizeof(filter*) * nfilters);
	r->number_of_filters = nfilters;
	r->id = id++;
	return r;
}
EXPORT_SYMBOL(make_rule);
