#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/in.h>
#include "spin_lock_queue.h"
#include "pdu.h"
#include "utils.h"

static sl_queue_node_t* make_spin_lock_queue_node(void* data) {
	sl_queue_node_t* node = kmalloc(sizeof(sl_queue_node_t), GFP_ATOMIC);
	if (!node) {
		print_error("Unable to create spin lock queue node.\n");
		return NULL;
	}
	memset(node, 0, sizeof(sl_queue_node_t));
	node->data = data;
	return node;
}

sl_queue_t* make_spin_lock_queue(void) {
	sl_queue_t* q = kmalloc(sizeof(sl_queue_t), GFP_ATOMIC);
	if (!q) {
		print_error("Unable to create spin lock queue.\n");
		return NULL;
	}
	memset(q, 0, sizeof(sl_queue_t));
	q->queue_lock = SPIN_LOCK_UNLOCKED;
	return q;
}

static void* free_spin_lock_queue_node(sl_queue_node_t* node) {
	void* data = node->data;
	kfree(node);
	return data;
}

void free_spin_lock_queue(sl_queue_t* q, void(*free_data_fn(void*))) {
	sl_queue_node_t* it;
	void *data;

	spin_lock_bh(&(q->queue_lock));
	it = q->first;
	while (it != NULL) {
		q->first = q->first->next;
		data = free_spin_lock_queue_node(it);
		if (data && free_data_fn) {
			free_data_fn(data);
		} else if (data)
			kfree(data);
		it = q->first;
	}
	spin_unlock_bh(&(q->queue_lock));
	kfree(q);
}

void spin_lock_queue_enqueue(sl_queue_t* q, void* data) {
	sl_queue_node_t* n = make_spin_lock_queue_node(data);
	if (!n)
		return;

	spin_lock_bh(&(q->queue_lock));
	if (!q->first) {
		q->first = n;
		q->last = n;
		spin_unlock_bh(&(q->queue_lock));
		return;
	}
	q->first->before = n;
	n->next = q->first;
	q->first = n;
	spin_unlock_bh(&(q->queue_lock));
	return;
}

void* spin_lock_queue_dequeue(sl_queue_t* q) {
	sl_queue_node_t* n;
	void* data;

	spin_lock_bh(&(q->queue_lock));
	if(q->first == NULL){
		spin_unlock_bh(&(q->queue_lock));
		return NULL;
	}

	if (q->first == q->last) {
		n = q->last;
		data = n->data;
		q->last = q->first = NULL;
		free_spin_lock_queue_node(n);
		spin_unlock_bh(&(q->queue_lock));
		return data;
	}
	n = q->last;
	data = n->data;
	q->last = q->last->before;
	free_spin_lock_queue_node(n);
	spin_unlock_bh(&(q->queue_lock));
	return data;
}
