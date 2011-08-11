#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include "rbtree.h"
#include "registry.h"
#include "registry_error.h"
#include "macros.h"

#define LOCK_REGISTRY pthread_mutex_lock(&handler_registry_lock)
#define UNLOCK_REGISTRY pthread_mutex_unlock(&handler_registry_lock)

pthread_mutex_t handler_registry_lock;
tree_root* handler_registry = NULL;

static void* handler_key(tree_node* n){
  return &(((handler_table_entry*) (n->node))->hid);
}

static int64_t handler_compare(void* keyA, void* keyB){
  uint16_t* a = (uint16_t*) keyA, *b = (uint16_t*) keyB;
  return (int64_t) (((int64_t) *a) - ((int64_t) *b));
}

void handler_registry_start(void){
  debug(I, "Starting registry...\n");
  handler_registry = new_rbtree(handler_key, handler_compare);
  pthread_mutex_init(&handler_registry_lock, NULL);
}

void handler_registry_stop(void){
  tree_iterator* it;
  handler_table_entry* ht;
  debug(I, "Stopping registry...\n");
  it = new_tree_iterator(handler_registry);
  while(tree_iterator_has_next(it)){
    ht = (handler_table_entry*) tree_iterator_next(it);
    free(ht);
  }
  destroy_iterator(it);
  destroy_rbtree(handler_registry);
  handler_registry = NULL;
  pthread_mutex_destroy(&handler_registry_lock);
}

uint32_t registry_register_handler(uint16_t hid, struct sockaddr addr){
  handler_table_entry* ht;

  if(!handler_registry)
    return H_ERROR_NOT_INITIALIZED;

  LOCK_REGISTRY;
  if(search_rbtree(* handler_registry, &hid)){
    UNLOCK_REGISTRY;
    return H_ERROR_DUPLICATE_ID;
  }
  UNLOCK_REGISTRY;

  ht = alloc(handler_table_entry, 1);

  ht->hid = hid;
  ht->addr = addr;
  LOCK_REGISTRY;
  rb_tree_insert(handler_registry, ht);
  UNLOCK_REGISTRY;
  debug(I, "Registered handler with id %u\n", hid);
  return 0;
}

uint32_t registry_unregister_handler(uint16_t hid){
  handler_table_entry* ht;
  LOCK_REGISTRY;
  ht = (handler_table_entry*) rb_tree_delete(handler_registry, &hid);
  if(ht){
    free(ht);
  }
  UNLOCK_REGISTRY;
  debug(I, "Unregistered handler with id %d\n", hid);
  return 0;
}

struct sockaddr* registry_get_handler_address(uint16_t hid){
  handler_table_entry* ht;
  LOCK_REGISTRY;
  ht = search_rbtree(* handler_registry, &hid);
  UNLOCK_REGISTRY;
  if(ht){
    debug(I, "Got handler with id %d\n", hid);
    return &(ht->addr);
  }
  return NULL;
}
