#ifndef __SPIN_LOCK_QUEUE_H__
#define __SPIN_LOCK_QUEUE_H__

#include <linux/spinlock.h>

typedef struct sl_queue_node{
	void* data;
	struct sl_queue_node* next;
	struct sl_queue_node* before;
}sl_queue_node_t;

typedef struct{
	sl_queue_node_t * first;
	sl_queue_node_t * last;
	spinlock_t queue_lock;
}sl_queue_t;

extern sl_queue_t* make_spin_lock_queue(void);
extern void free_spin_lock_queue(sl_queue_t* q, void (*free_data_fn(void*)));
extern void spin_lock_queue_enqueue(sl_queue_t* q, void* data);
extern void* spin_lock_queue_dequeue(sl_queue_t* q);

#endif
