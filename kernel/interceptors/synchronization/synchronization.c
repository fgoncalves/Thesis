/*
 * This is the synchronization interceptor. It will register rules with two filters. One at
 * PREROUTING and another at POSTROUTING.
 *
 * Prior to this module I had code to recalculate UDP checksums. However, that was causing
 * some trouble. After delving into the problem, I've found that at this stage (IP layer)
 * one can simply put udp checksum equal to zero and the NIC will calculate it for us. Well
 * at least modern hardware is capable of such. Therefore I simply removed udp checksum
 * recalculation from the code.
 * */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h> 
#include <linux/netfilter.h> 
#include <linux/netfilter_ipv4.h> 
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include "interceptor.h"
#include "pdu.h"
#include "utils.h"

#define INTERCEPTOR_NAME "synchronization"

unsigned int pre_routing_hook(filter_specs* sp, unsigned int hooknum,
		struct sk_buff* skb, const struct net_device* in,
		const struct net_device *out, int(*okfn)(struct sk_buff*)) {
	struct iphdr* iph;
	__tp(pdu)* pdu;
	control_byte* cb;

	if (!skb)
		return NF_ACCEPT;

	if (!skb_network_header(skb) || !skb_transport_header(skb))
		return NF_ACCEPT;

	iph = ip_hdr(skb);
	switch (iph->protocol) {
	case IPPROTO_UDP:
		cb = control_byte_skb(skb);

		if (cb->synched)
			return NF_ACCEPT;

		cb->synched = 1;
		if (cb->l3_aggregated) {
			cb = (control_byte*) (((char*) iph) + ((((iph->ihl << 2)
					+ sizeof(struct udphdr)) << 1) + sizeof(control_byte)));
			pdu = (__tp(pdu)*) (((char*) iph) + (((iph->ihl << 2)
					+ sizeof(struct udphdr) + sizeof(control_byte)) << 1));
			cb->synched = 1;
		} else
			pdu = pdu_skb(skb);

		swap_pdu_byte_order(pdu);

		//Application does all the synchronization
		if (pdu->flags.app_synch)
			return NF_ACCEPT;

		pdu->timestamp = get_kernel_current_time() - pdu->timestamp;

		swap_pdu_byte_order(pdu);
		break;
	default:
		printk("\tProtocol not supported.\n");
	}
	return NF_ACCEPT;
}

unsigned int post_routing_hook(filter_specs* sp, unsigned int hooknum,
		struct sk_buff* skb, const struct net_device* in,
		const struct net_device *out, int(*okfn)(struct sk_buff*)) {
	struct iphdr* iph;
	__tp(pdu)* pdu;
	control_byte* cb;

	if (!skb)
		return NF_ACCEPT;

	if (!skb_network_header(skb) || !skb_transport_header(skb))
		return NF_ACCEPT;

	iph = ip_hdr(skb);

	switch (iph->protocol) {
	case IPPROTO_UDP:
		cb = control_byte_skb(skb);

		//Do not check if cb is synchronized.
		//Packets leaving this node must always be synchronized

		if (cb->l3_aggregated) {
			cb = (control_byte*) (((char*) iph) + ((((iph->ihl << 2)
					+ sizeof(struct udphdr)) << 1) + sizeof(control_byte)));
			pdu = (__tp(pdu)*) (((char*) iph) + (((iph->ihl << 2)
					+ sizeof(struct udphdr) + sizeof(control_byte)) << 1));
		} else
			pdu = pdu_skb(skb);

		swap_pdu_byte_order(pdu);

		//Application does all the synchronization
		if (pdu->flags.app_synch)
			return NF_ACCEPT;

		pdu->timestamp = get_kernel_current_time() - pdu->timestamp;

		swap_pdu_byte_order(pdu);

		break;
	default:
		printk("\tProtocol not supported.\n");
	}

	return NF_ACCEPT;
}

interceptor_descriptor synch_desc;

rule* create_rule_for_specs(filter_specs* sp) {
	rule* r = make_rule(2);
	filter* pre_routing_filter = create_filter(FILTER_PRIO_FIRST, sp, r,
			FILTER_AT_PRE_RT, pre_routing_hook);
	filter* post_routing_filter = create_filter(FILTER_PRIO_LAST, sp, r,
			FILTER_AT_POST_RT, post_routing_hook);
	(r->filters)[0] = pre_routing_filter;
	(r->filters)[1] = post_routing_filter;
	r->interceptor = &synch_desc;
	return r;
}

int __init init_module() {
	synch_desc.name = INTERCEPTOR_NAME;
	synch_desc.create_rule_for_specs_cb = create_rule_for_specs;

	register_interceptor(&synch_desc);

	printk(KERN_INFO "Synchronization module loaded.\n");

	return 0;
}

void __exit cleanup_module() {
	if (!unregister_interceptor(INTERCEPTOR_NAME))
		printk(
				"Unable to unregister synchronization interceptor. Interceptor framework will panic soon.\n");

printk(KERN_INFO "Synchronization module unloaded.\n");
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Frederico Gonçalves, [frederico.lopes.goncalves@gmail.com]");
MODULE_DESCRIPTION("Synchronization interceptor.");

