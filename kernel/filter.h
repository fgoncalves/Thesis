#ifndef __FILTER_H__
#define __FILTER_H__

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include "rule.h"

#define FILTER_BY_L4_PROTO 0x01
#define FILTER_BY_DST_PORT 0x02
#define FILTER_BY_SRC_PORT 0x04
#define FILTER_BY_DST_ADDR 0x08
#define FILTER_BY_SRC_ADDR 0x10

#define FILTER_PRIO_FIRST 0x00
#define FILTER_PRIO_UNSPECIFIED 0x7F
#define FILTER_PRIO_LAST 0xFF

#define FILTER_AT_LOCAL_IN 0x01
#define FILTER_AT_LOCAL_OUT 0x02
#define FILTER_AT_FWD 0x04
#define FILTER_AT_POST_RT 0x08
#define FILTER_AT_PRE_RT 0x10

//Must set all ops in nbo.
typedef struct sfspecs{
  uint8_t filter_by;
  __u8 protocol;
  __be32 dst_addr;
  __be32 src_addr;
  __be16 dst_port;
  __be16 src_port;
}filter_specs;

typedef struct sfilter{
  uint8_t priority;
  uint8_t filter_at;
  struct srule* r; //rule that holds this filter
  struct sfspecs* filter_specs;
  unsigned int (*apply_filter)(filter_specs*, unsigned int, struct sk_buff*, const struct net_device*, const struct net_device *, int (*okfn)(struct sk_buff*));
}filter;

#define apply_filter(filter, hooknum, skb, in, out, okfn)	\
  (filter)->apply_filter((filter)->filter_specs, (hooknum), (skb), (in), (out), (okfn))

extern filter* create_filter(uint8_t prio, filter_specs* sp, struct srule* r, uint8_t filter_at,  unsigned int (*nf_hook)(filter_specs*, unsigned int, struct sk_buff*,
												       const struct net_device*, const struct net_device *, 
											     int (*okfn)(struct sk_buff*)));
extern void free_filter(filter *fg);
extern filter_specs* create_filter_specs(uint8_t filter_by, __be32 dst_addr, __be32 src_addr, __be16 dst_port, __be16 src_port, int proto);
extern void free_filter_specs(filter_specs* sp);
#endif
