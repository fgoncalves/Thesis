/*
 * Before reading this file, I recommend reading klist.c, filter.c and understanding the basics
 * of Netfilters.
 *
 * This file implements an array of filter chains. Such array stores 5 filter_chains and
 * corresponds to the network entry points: POSTROUTING, FORWARD, PREROUTING, LOCAL IN and
 * LOCAL OUT.
 *
 * Each filter chain is a klist, where each node is a filter which should be applied at
 * that specific network entry point.
 * */

#include <linux/in.h>
#include <linux/inet.h>
#include "chains.h"
#include "filter.h"

filter_chain chains[CHAINS_SIZE];

/*
 * Initialize each chain.
 *
 * Klist's will be created here.
 * */
void init_chains(void) {
	uint8_t i;
	memset(chains, 0, sizeof(filter_chain) * CHAINS_SIZE);
	for (i = 0; i < CHAINS_SIZE; i++)
		chains[i].filters = make_klist();
}

static void __unregister_filter(int chain, filter* f) {
	filter_chain c = chains[chain];
	klist_iterator* it = make_klist_iterator(c.filters);
	filter* t;
	while (klist_iterator_has_next(it)) {
		t = (filter*) (klist_iterator_peek(it))->data;
		if (t == f) {
			klist_iterator_remove_current(it);
			chains[chain].len--;
			break;
		}
		klist_iterator_next(it);
	}
	free_klist_iterator(it);
}

/*Unregister the filter pointed by 'f'*/
void unregister_filter(filter* f) {
	switch (f->filter_at) {
	case FILTER_AT_LOCAL_IN:
		__unregister_filter(LOCAL_IN_CHAIN, f);
		return;
	case FILTER_AT_LOCAL_OUT:
		__unregister_filter(LOCAL_OUT_CHAIN, f);
		return;
	case FILTER_AT_FWD:
		__unregister_filter(FORWARD_CHAIN, f);
		return;
	case FILTER_AT_POST_RT:
		__unregister_filter(POST_ROUTING_CHAIN, f);
		return;
	case FILTER_AT_PRE_RT:
		__unregister_filter(PRE_ROUTING_CHAIN, f);
		return;
	default:
		printk(
				"** Unable to unregister filter because of wrong point of filtering.\n");
	}
	return;
}

static int __register_filter(int chain, filter* f) {
	filter* temp;
	klist_iterator* it;

	if (chains[chain].len == MAX_FILTERS_PER_CHAIN) {
		printk(
				"Unable to register filter. Chain already has the maximum number of possible filters.\n");
		return 0;
	}

	it = make_klist_iterator(chains[chain].filters);
	while (klist_iterator_has_next(it)) {
		temp = (filter*) (klist_iterator_peek(it))->data;
		if (f->priority > temp->priority) {
			klist_iterator_insert_before(it, f);
			chains[chain].len++;
			free_klist_iterator(it);
			return 1;
		}
		klist_iterator_next(it);
	}
	free_klist_iterator(it);
	chains[chain].len++;
	add_klist_node_to_klist(chains[chain].filters, make_klist_node(f));
	return 1;
}

/*
 * Register the filter pointer by 'f'. 'f' will not be copied, so do not destroy it.
 * Also, 'f' must be correctly filled before calling this function. I recommend using
 * the filter creation function declared in filter.h.
 * */
int register_filter(filter* f) {
	switch (f->filter_at) {
	case FILTER_AT_LOCAL_IN:
		if (!__register_filter(LOCAL_IN_CHAIN, f))
			return 0;
		return 1;
	case FILTER_AT_LOCAL_OUT:
		if (!__register_filter(LOCAL_OUT_CHAIN, f))
			return 0;
		return 1;
	case FILTER_AT_FWD:
		if (!__register_filter(FORWARD_CHAIN, f))
			return 0;
		return 1;
	case FILTER_AT_POST_RT:
		if (!__register_filter(POST_ROUTING_CHAIN, f))
			return 0;
		return 1;
	case FILTER_AT_PRE_RT:
		if (!__register_filter(PRE_ROUTING_CHAIN, f))
			return 0;
		return 1;
	default:
		printk(
				"** Unable to register filter because of wrong point of filtering.\n");
	}
	return 0;
}

/*
 * This is just a helper function to get a given chain. I recommend it's use because
 * future versions may include synchronized access to 'chains', but this prototype will
 * remain the same.
 *
 * DO NOT use integers directly as arguments to this function. Use the constants defined in
 * chains.h.
 * */
filter_chain get_chain(int chain) {
	return chains[chain];
}
