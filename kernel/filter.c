/*
 * Before reading this file, I recommend understanding of Netfilter hooks and decisions, such as NF_DROP and NF_ACCEPT.
 *
 * This file implements the basic filter structure and filter_specs.
 *
 * A filter is just a structure which has an entry point (FORWARD, PREROUTING, POSTROUTING,
 * LOCAL IN, LOCAL OUT), some specifications and an interceptor callback.
 * Here specifications are the network packet fields to
 * which the filter must be applied. Current implementation supports ip protocol, source and
 * destination address, source and destination port. However, the rest of the code only
 * accounts for UDP protocol, but it can be easily extended.
 *
 * Filter should also have a priority. For example, synchronization should happen before anything else in PREROUTING.
 * This is achieved by passing FILTER_PRIO_FIRST as filter priority.
 *
 * Therefore, a filter with an entry point at LOCAL IN and a specification of source address
 * 192.168.0.1, will be applied to every packet destined to us (LOCAL IN) with a source address equal to 192.168.0.1.
 *
 * Applying a filter consists in calling the interceptor registered callback function. This callback should return
 * a netfilter decision.
 *
 * All memory allocation is done using kmalloc with GFP_ATOMIC.
 * */

#include <linux/kernel.h>
#include <linux/string.h>
#include "filter.h"

/*
 * Create a filter specification. 'filter_by' should be filled by oring the FILTER_BY_* constants in filter.h.
 *
 * For example, if filter_by = FILTER_BY_L4_PROTO | FILTER_BY_DST_PORT, a call like create_filter_specs(filter_by, 4332,
 * 1234, 57843, 44, IPPROTO_UDP) will create a specification which will filter traffic with UDP protocol and destination port 57843.
 * */
filter_specs* create_filter_specs(uint8_t filter_by, __be32 dst_addr,
		__be32 src_addr, __be16 dst_port, __be16 src_port, int proto) {
	filter_specs* fs =
			(filter_specs*) kmalloc(sizeof(filter_specs), GFP_ATOMIC);
	if (!fs) {
		printk("%s:%u: kmalloc failed. Unnable to create filter.\n", __FILE__,
				__LINE__);
		return NULL;
	}

	memset(fs, 0, sizeof(filter_specs));
	fs->filter_by = filter_by;
	fs->dst_addr = dst_addr;
	fs->src_addr = src_addr;
	fs->dst_port = dst_port;
	fs->src_port = src_port;
	fs->protocol = proto;

	return fs;
}

void free_filter_specs(filter_specs* sp) {
	kfree(sp);
}

/* Create a filter. This should be called by interceptors.*/
filter* create_filter(uint8_t prio, filter_specs* sp, struct srule* r, uint8_t filter_at, unsigned int(*nf_hook)(
		filter_specs*, unsigned int, struct sk_buff*, const struct net_device*,
		const struct net_device *, int(*okfn)(struct sk_buff*))) {
	filter* f = (filter*) kmalloc(sizeof(filter), GFP_ATOMIC);
	if (!f) {
		printk("%s:%u: kmalloc failed.\n", __FILE__, __LINE__);
		return NULL;
	}

	f->priority = prio;
	f->filter_specs = sp;
	f->apply_filter = nf_hook;
	f->r = r;
	f->filter_at = filter_at;
	return f;
}
EXPORT_SYMBOL(create_filter);

/*WARNING: This will not free the specification for filter 'fg'.*/
void free_filter(filter *fg) {
	kfree(fg);
}

