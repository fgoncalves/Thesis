#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/netdevice.h> 
#include <linux/netfilter.h> 
#include <linux/netfilter_ipv4.h> 
#include <linux/skbuff.h> 
#include <linux/in.h>

#include "pdu.h"

static int drop_packets[256] = {0};
static int dpcount = 0;

module_param_array(drop_packets, int, &dpcount, 0000);
MODULE_PARM_DESC(drop_packets, "An array of integers with the sequence numberr of packets that should be drop once.");

#ifdef NF_IP_POST_ROUTING
#define ip_post_routing NF_IP_POST_ROUTING
#else
#define ip_post_routing NF_INET_POST_ROUTING
#endif

static struct nf_hook_ops nf_ip_post_routing;

//Left to right
uint8_t bit_field[32] = {0};

#define set_bit(SEQ)  (bit_field[(SEQ) / 8] |= (128 >> ((SEQ) % 8)))
#define get_bit(SEQ)  (bit_field[(SEQ) / 8] & (128 >> ((SEQ) % 8)))

void dump_drop_packets(void){
  int i;
  for(i = 0; i < dpcount; i++)
    printk("%d ", drop_packets[i]);
  printk("\n");
}

uint8_t drop_it(int seq){
  int i;
  for(i = 0; i < dpcount; i++)
    if(drop_packets[i] == seq && get_bit(seq))      
      return 1;
  return 0;
}

static unsigned int nf_ip_post_routing_hook(unsigned int hooknum,
					    struct sk_buff *skb, const struct net_device *in,
					    const struct net_device *out, int(*okfn)(struct sk_buff*)) {
  struct iphdr* iph = ip_hdr(skb);
  __tp(pdu) *pdu;

  if(iph->protocol == IPPROTO_UDP){
    pdu = (__tp(pdu)*) (((char*) iph) + (iph->ihl << 2) + sizeof(struct udphdr) + sizeof(control_byte));
    if(drop_it(pdu->seq)){
      printk(KERN_EMERG "Dropping packet %d\n", pdu->seq);
      set_bit(pdu->seq);
      kfree_skb(skb);
      return NF_STOLEN;
    }
  }

  return NF_ACCEPT;
}

int __init init_module(void) {
  nf_ip_post_routing.hook = nf_ip_post_routing_hook;
  nf_ip_post_routing.pf = PF_INET;
  nf_ip_post_routing.hooknum = ip_post_routing;
  nf_ip_post_routing.priority = NF_IP_PRI_FIRST;
  nf_register_hook(&nf_ip_post_routing);

  dump_drop_packets();
  return 0;
}

void __exit cleanup_module(void) {
  nf_unregister_hook(&nf_ip_post_routing);
}

MODULE_LICENSE("GPL");
