/*
 * This file implements a generic double linked list with iterators.
 *
 * Iterators support remove and add operations. Although current implementation only
 * supports adding before the current element.
 *
 * All memory allocation is done using kmalloc with GFP_ATOMIC, so this code is safe
 * to use from an interrupt context. Netfilter hooks are interrupts!!!
 *
 * Other kernel modules can use this structure, because all relevant functions are exported.
 * */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "klist.h"

#define kalloc(type, how_many)				\
	(type*) kmalloc(sizeof(type) * how_many, GFP_ATOMIC)

#define kalloc_error(str)									\
	printk("%s in %s:%d: kmalloc failed. %s\n", __FILE__, __FUNCTION__, __LINE__, (str))

static void free_klist_node(klist_node* kn) {
	if (!kn)
		kfree(kn);
}

/*
 * Create a list node with a pointer to 'data'.
 * There is no internal copy of 'data'. This function will
 * create a node pointing to the same address as 'data'.
 * */
klist_node* make_klist_node(void* data) {
	klist_node* kn = kalloc(klist_node, 1);
	if (!kn) {
		kalloc_error("Unnable to create klist node.\n");
		return NULL;
	}
	memset(kn, 0, sizeof(klist_node));
	kn->data = data;
	return kn;
}
EXPORT_SYMBOL(make_klist_node);

/*
 * Create an empty list.
 * */
klist* make_klist(void) {
	klist* kl = kalloc(klist, 1);
	if (!kl) {
		kalloc_error("Unnable to create klist.\n");
		return NULL;
	}
	memset(kl, 0, sizeof(klist));
	return kl;
}
EXPORT_SYMBOL(make_klist);

/*
 * Add a new node to the given list.
 * */
void add_klist_node_to_klist(klist *kl, klist_node* kn) {
	if (kl->first == NULL) {
		kl->first = kl->last = kn;
		return;
	}

	kn->back = kl->last;
	kl->last->front = kn;
	kl->last = kn;
}
EXPORT_SYMBOL(add_klist_node_to_klist);

/*
 * Remove the node pointed by 'kn' from the given list.
 * */
void remove_klist_node_from_klist(klist* kl, klist_node* kn) {
	if (!kn || !kl)
		return;

	if (kl->first == kn && kn == kl->last) {
		kl->first = kl->last = NULL;
		free_klist_node(kn);
		return;
	}

	if (kl->first == kn) {
		kl->first = kn->front;
		kl->first->back = NULL;
		free_klist_node(kn);
		return;
	}

	if (kl->last == kn) {
		kl->last = kn->back;
		kl->last->front = NULL;
		free_klist_node(kn);
		return;
	}

	kn->back->front = kn->front;
	kn->front->back = kn->back;
	free_klist_node(kn);
}
EXPORT_SYMBOL(remove_klist_node_from_klist);

/*
 * Free a given list.
 *
 * Since this implementation was created so any kind of data could be stored in a list,
 * there is no way of knowing how to free such data. That's the reason this function
 * takes a pointer to another function which should do it. In other words, free_klist_node_data_cb
 * is a callback function which should free data in each list node.
 *
 * If such data doesn't need to be freed, then NULL can be passed to this function.
 * */
void free_klist(klist* kl, void(*free_klist_node_data_cb)(void* data)) {
	klist_node* it;
	if (!kl)
		return;

	it = kl->first;
	while (it != NULL) {
		kl->first = it->front;
		if (free_klist_node_data_cb)
			free_klist_node_data_cb(it->data);
		kfree(it);
		it = kl->first;
	}

	kfree(kl);
}
EXPORT_SYMBOL(free_klist);

/*
 * Create an iterator for the given list:
 *
 * WARNING: Using iterators is a good practice and may be used everywhere in code.
 * However, you MUST destroy the iterator after using it or else memory will not be freed,
 * which will eventually end in a system failure.
 * */
klist_iterator* make_klist_iterator(klist* kl) {
	klist_iterator* kit;

	if (!kl)
		return NULL;

	kit = kalloc(klist_iterator, 1);
	if (!kit) {
		kalloc_error("Unnable to create klist iterator.\n");
		return NULL;
	}
	kit->cur = kl->first;
	kit->list = kl;
	return kit;
}
EXPORT_SYMBOL(make_klist_iterator);

/*
 * Check if there are more elements to be iterated.
 * */
uint8_t klist_iterator_has_next(klist_iterator* ki) {
	return (ki && ki->cur);
}
EXPORT_SYMBOL(klist_iterator_has_next);

/*
 * Return the current node pointed by the given iterator and advance such iterator
 * to the next node.
 * */
klist_node* klist_iterator_next(klist_iterator* ki) {
	klist_node* result;
	if (!ki || !ki->cur)
		return NULL;

	result = ki->cur;
	ki->cur = ki->cur->front;
	return result;
}
EXPORT_SYMBOL(klist_iterator_next);

/*
 * Use it!! Do not be afraid.
 * */
void free_klist_iterator(klist_iterator* ki) {
	kfree(ki);
}
EXPORT_SYMBOL(free_klist_iterator);

/*
 * Same as klist_iterator_next. However the iterator will not be advanced to the next
 * node.
 * */
klist_node* klist_iterator_peek(klist_iterator* ki) {
	return ki->cur;
}
EXPORT_SYMBOL(klist_iterator_peek);

/*
 * Remove the current node pointed by the given iterator and advance such iterator to the
 * next node.
 * */
void* klist_iterator_remove_current(klist_iterator* ki) {
	void* dt;
	klist_node* kn;
	if (ki->cur) {
		dt = ki->cur->data;
		kn = ki->cur;
		ki->cur = ki->cur->front;
		remove_klist_node_from_klist(ki->list, kn);
		return dt;
	}
	return NULL;
}
EXPORT_SYMBOL(klist_iterator_remove_current);

/*
 * Insert a new node with 'data', before the node pointed by the given iterator.
 * */
void klist_iterator_insert_before(klist_iterator* ki, void* data) {
	klist_node* new = make_klist_node(data);
	if (ki->cur == ki->list->first)
		ki->list->first = new;
	new->front = ki->cur;
	new->back = ki->cur->back;
	if (ki->cur->back)
		ki->cur->back->front = new;
	ki->cur->back = new;
}
EXPORT_SYMBOL(klist_iterator_insert_before);
