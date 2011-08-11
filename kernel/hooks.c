/* Before reading this file I recommend having a knowledge of netfilter hooks and reading the
 * file filter.c.
 *
 * This file implements 5 netfilter hooks. One for each possible entry point.
 *
 * Each hook will try to match the packet against each filter for the corresponding entry point.
 * For each packet matched this way, the filter interceptor callback will be called and a netfilter
 * decision should be returned.
 *
 * If this decision is different from NF_ACCEPT then no more matching will be done and the
 * same decision will be returned by such hook.
 *
 * For example, packet aggregation callback will buffer the packet and return NF_STOLEN.
 * In this case, matching will stop and NF_STOLEN will be returned by the corresponding hook.
 * */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/netdevice.h> 
#include <linux/netfilter.h> 
#include <linux/netfilter_ipv4.h> 
#include <linux/skbuff.h> 
#include <linux/in.h>

#include "hooks.h"
#include "chains.h"
#include "filter.h"
#include "pdu.h"

/*Each matching function returns 0 if match fails and 1 otherwise.*/

static uint8_t match_src_addr(filter* f, struct sk_buff* skb) {
	filter_specs *sp = f->filter_specs;
	struct iphdr* ip_header = ip_hdr(skb);

	if ((sp->filter_by & FILTER_BY_SRC_ADDR) && (sp->src_addr
			!= ip_header->saddr))
		return 0;
	return 1;
}

static uint8_t match_dst_addr(filter* f, struct sk_buff* skb) {
	filter_specs* sp = f->filter_specs;
	struct iphdr* ip_header = ip_hdr(skb);

	if ((sp->filter_by & FILTER_BY_DST_ADDR) && (sp->dst_addr
			!= ip_header->daddr))
		return 0;
	return 1;
}

static uint8_t match_proto_and_ports(filter* f, struct sk_buff* skb) {
	filter_specs* sp = f->filter_specs;
	struct iphdr* ip_header = ip_hdr(skb);
	struct udphdr* udp_header;

	if (sp->filter_by & FILTER_BY_L4_PROTO) {
		if (ip_header->protocol != sp->protocol)
			return 0;
		else {
			switch (sp->protocol) {
			case IPPROTO_UDP:
				udp_header = (struct udphdr*) (((char*) ip_header)
						+ (ip_header->ihl << 2));
				if ((sp->filter_by & FILTER_BY_DST_PORT) && (sp->dst_port
						!= udp_header->dest))
					return 0;
				if ((sp->filter_by & FILTER_BY_SRC_PORT) && (sp->src_port
						!= udp_header->source))
					return 0;
				break;
			}
		}
	}
	return 1;
}

static uint8_t match_filter(filter* f, struct sk_buff* skb) {
	if (!match_src_addr(f, skb))
		return 0;

	if (!match_dst_addr(f, skb))
		return 0;

	if (!match_proto_and_ports(f, skb))
		return 0;
	return 1;
}

/*This is to avoid some kernel compatibility issues. Use the lower case versions.*/
#ifdef NF_IP_LOCAL_OUT
#define ip_local_out NF_IP_LOCAL_OUT
#else
#define ip_local_out NF_INET_LOCAL_OUT
#endif

#ifdef NF_IP_LOCAL_IN
#define ip_local_in NF_IP_LOCAL_IN
#else
#define ip_local_in NF_INET_LOCAL_IN
#endif

#ifdef NF_IP_FORWARD
#define ip_forward NF_IP_FORWARD
#else
#define ip_forward NF_INET_FORWARD
#endif

#ifdef NF_IP_PRE_ROUTING
#define ip_pre_routing NF_IP_PRE_ROUTING
#else
#define ip_pre_routing NF_INET_PRE_ROUTING
#endif

#ifdef NF_IP_POST_ROUTING
#define ip_post_routing NF_IP_POST_ROUTING
#else
#define ip_post_routing NF_INET_POST_ROUTING
#endif

static struct nf_hook_ops nf_ip_local_out;
static struct nf_hook_ops nf_ip_local_in;
static struct nf_hook_ops nf_ip_forward;
static struct nf_hook_ops nf_ip_pre_routing;
static struct nf_hook_ops nf_ip_post_routing;

static unsigned int nf_ip_local_out_hook(unsigned int hooknum,
		struct sk_buff *skb, const struct net_device *in,
		const struct net_device *out, int(*okfn)(struct sk_buff*)) {
	filter_chain c = get_chain(LOCAL_OUT_CHAIN);
	klist_iterator* it;
	filter* f;
	unsigned int retval;

	if (c.len == 0)
		return NF_ACCEPT;

	it = make_klist_iterator(c.filters);
	while (klist_iterator_has_next(it)) {
		f = (filter*) (klist_iterator_next(it))->data;
		if (match_filter(f, skb)) {
			retval = apply_filter(f, hooknum, skb, in, out, okfn);
			if (retval != NF_ACCEPT) {
				free_klist_iterator(it);
				return retval;
			}
		}
	}
	free_klist_iterator(it);

	return NF_ACCEPT;
}

static unsigned int nf_ip_local_in_hook(unsigned int hooknum,
		struct sk_buff *skb, const struct net_device *in,
		const struct net_device *out, int(*okfn)(struct sk_buff*)) {
	filter_chain c = get_chain(LOCAL_IN_CHAIN);
	klist_iterator* it;
	filter* f;
	unsigned int retval;

	if (c.len == 0)
		return NF_ACCEPT;

	it = make_klist_iterator(c.filters);
	while (klist_iterator_has_next(it)) {
		f = (filter*) (klist_iterator_next(it))->data;
		if (match_filter(f, skb)) {
			retval = apply_filter(f, hooknum, skb, in, out, okfn);
			if (retval != NF_ACCEPT) {
				free_klist_iterator(it);
				return retval;
			}
		}
	}
	free_klist_iterator(it);

	return NF_ACCEPT;
}

static unsigned int nf_ip_forward_hook(unsigned int hooknum,
		struct sk_buff *skb, const struct net_device *in,
		const struct net_device *out, int(*okfn)(struct sk_buff*)) {
	filter_chain c = get_chain(FORWARD_CHAIN);
	klist_iterator* it;
	filter* f;
	unsigned int retval;

	if (c.len == 0)
		return NF_ACCEPT;

	it = make_klist_iterator(c.filters);
	while (klist_iterator_has_next(it)) {
		f = (filter*) (klist_iterator_next(it))->data;
		if (match_filter(f, skb)) {
			retval = apply_filter(f, hooknum, skb, in, out, okfn);
			if (retval != NF_ACCEPT) {
				free_klist_iterator(it);
				return retval;
			}
		}
	}
	free_klist_iterator(it);

	return NF_ACCEPT;
}

static unsigned int nf_ip_pre_routing_hook(unsigned int hooknum,
		struct sk_buff *skb, const struct net_device *in,
		const struct net_device *out, int(*okfn)(struct sk_buff*)) {
	filter_chain c = get_chain(PRE_ROUTING_CHAIN);
	klist_iterator* it;
	filter* f;
	unsigned int retval;

	if (c.len == 0)
		return NF_ACCEPT;

	it = make_klist_iterator(c.filters);
	while (klist_iterator_has_next(it)) {
		f = (filter*) (klist_iterator_next(it))->data;
		if (match_filter(f, skb)) {
			retval = apply_filter(f, hooknum, skb, in, out, okfn);
			if (retval != NF_ACCEPT) {
				free_klist_iterator(it);
				return retval;
			}
		}
	}
	free_klist_iterator(it);

	return NF_ACCEPT;
}

static unsigned int nf_ip_post_routing_hook(unsigned int hooknum,
		struct sk_buff *skb, const struct net_device *in,
		const struct net_device *out, int(*okfn)(struct sk_buff*)) {
	filter_chain c = get_chain(POST_ROUTING_CHAIN);
	klist_iterator* it;
	filter* f;
	unsigned int retval;

	if (c.len == 0)
		return NF_ACCEPT;

	it = make_klist_iterator(c.filters);
	while (klist_iterator_has_next(it)) {
		f = (filter*) (klist_iterator_next(it))->data;
		if (match_filter(f, skb)) {
			retval = apply_filter(f, hooknum, skb, in, out, okfn);
			if (retval != NF_ACCEPT) {
				free_klist_iterator(it);
				return retval;
			}
		}
	}
	free_klist_iterator(it);

	return NF_ACCEPT;
}

void init_hooks(void) {
	nf_ip_local_out.hook = nf_ip_local_out_hook;
	nf_ip_local_out.pf = PF_INET;
	nf_ip_local_out.hooknum = ip_local_out;
	nf_ip_local_out.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nf_ip_local_out);

	nf_ip_local_in.hook = nf_ip_local_in_hook;
	nf_ip_local_in.pf = PF_INET;
	nf_ip_local_in.hooknum = ip_local_in;
	nf_ip_local_in.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nf_ip_local_in);

	nf_ip_forward.hook = nf_ip_forward_hook;
	nf_ip_forward.pf = PF_INET;
	nf_ip_forward.hooknum = ip_forward;
	nf_ip_forward.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nf_ip_forward);

	nf_ip_pre_routing.hook = nf_ip_pre_routing_hook;
	nf_ip_pre_routing.pf = PF_INET;
	nf_ip_pre_routing.hooknum = ip_pre_routing;
	nf_ip_pre_routing.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nf_ip_pre_routing);

	nf_ip_post_routing.hook = nf_ip_post_routing_hook;
	nf_ip_post_routing.pf = PF_INET;
	nf_ip_post_routing.hooknum = ip_post_routing;
	nf_ip_post_routing.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nf_ip_post_routing);
}

void free_hooks(void) {
	nf_unregister_hook(&nf_ip_local_out);
	nf_unregister_hook(&nf_ip_local_in);
	nf_unregister_hook(&nf_ip_forward);
	nf_unregister_hook(&nf_ip_pre_routing);
	nf_unregister_hook(&nf_ip_post_routing);
}
