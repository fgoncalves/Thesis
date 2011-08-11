#ifndef __CHAINS_H__
#define __CHAINS_H__

#include "filter.h"
#include "klist.h"

#define MAX_FILTERS_PER_CHAIN 10

/*
 * Notice that this structure will be accessed from interrupt context in netfilter hooks.
 * Thus, having the 'len' attribute permits a more faster way of telling if the chain is empty
 * or not.
 * */
typedef struct {
  klist* filters;
  uint8_t len;
}filter_chain;

//Used to identify the chains in the 'chains' array. Read chains.c for more information.
#define LOCAL_OUT_CHAIN 0
#define LOCAL_IN_CHAIN 1
#define FORWARD_CHAIN 2
#define PRE_ROUTING_CHAIN 3
#define POST_ROUTING_CHAIN 4

//Change this and kernel will die...
#define CHAINS_SIZE 5

extern void init_chains(void);
extern int register_filter(filter* f);
extern void unregister_filter(filter* f);
extern filter_chain get_chain(int chain);
#endif
