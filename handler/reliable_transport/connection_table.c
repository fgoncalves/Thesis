#include "connection_table.h"

static void* connection_key(struct stree_node* node) {
	rt_connection* c = (rt_connection*) node->node;
	return &(c->buff->hid);
}

static int64_t connection_compare(void* keyA, void* keyB) {
	uint16_t* a = (uint16_t*) keyA, *b = (uint16_t*) keyB;
	return ((int64_t) *a) - ((int64_t) *b);
}

connection_table* mkconnection_table() {
	return new_rbtree(connection_key, connection_compare);
}

void free_connection_table(connection_table* ct) {
	if (ct) {
		tree_iterator *it = new_tree_iterator(ct);
		while (tree_iterator_has_next(it)) {
			rt_connection* c = (rt_connection*) tree_iterator_next(it);
			free_rtconnection(c);
		}
		destroy_iterator(it);
		destroy_rbtree(ct);
		ct = NULL;
	}
}

rt_connection* get_connection(connection_table* ct, uint16_t hid, uint32_t window){
	rt_connection *rt = search_rbtree(*ct, &hid);
	if(rt)
		return rt;

	debug(I, "Connection with handler %d not found. Creating it...\n", hid);

	rt = mkrtconnection(window, hid);
	rb_tree_insert(ct, rt);
	return rt;
}

void remove_connection(connection_table* ct, uint16_t hid){
	rt_connection* rt = (rt_connection*) rb_tree_delete(ct, &hid);
	if(rt)
		free_rtconnection(rt);
}
