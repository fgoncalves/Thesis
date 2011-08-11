#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "macros.h"
#include "rbtree.h"
#include "transport_config.h"

static tree_root* transport_config = NULL;

typedef struct {
	char* transport_type;
	tree_root* configuration;
} transport_config_node;

typedef struct {
	char* name;
	char* value;
} parameter;

static void* transport_config_key(struct stree_node* node) {
	transport_config_node* n = (transport_config_node*) (node->node);
	return n->transport_type;
}

static int64_t transport_config_compare(void* keyA, void* keyB) {
	char* a = (char*) keyA, *b = (char*) keyB;
	return strcmp(a, b);
}

static void* transport_config_parameter_key(struct stree_node* node) {
	parameter* n = (parameter*) (node->node);
	return n->name;
}

static int64_t transport_config_parameter_compare(void* keyA, void* keyB) {
	char* a = (char*) keyA, *b = (char*) keyB;
	return strcmp(a, b);
}

static void free_transport_config_parameters(tree_root* rt) {
	tree_iterator* it = new_tree_iterator(rt);
	while (tree_iterator_has_next(it)) {
		parameter* p = tree_iterator_next(it);
		free(p->name);
		free(p->value);
		free(p);
	}
	destroy_iterator(it);
	destroy_rbtree(rt);
}

uint8_t init_transport_config() {
	transport_config = new_rbtree(transport_config_key,
			transport_config_compare);
}

void free_transport_config() {
	tree_iterator* it = new_tree_iterator(transport_config);
	while (tree_iterator_has_next(it)) {
		transport_config_node* n = tree_iterator_next(it);
		free(n->transport_type);
		free_transport_config_parameters(n->configuration);
		free(n);
	}
	destroy_iterator(it);
	destroy_rbtree(transport_config);
	transport_config = NULL;
}

void set_transport_parameter(char* transport_type, char* name, char* value) {
	transport_config_node* n = NULL;
	if ((n = (transport_config_node*) search_rbtree(*transport_config,
			transport_type)) == NULL) {
		n = alloc(transport_config_node, 1);
		n->transport_type = alloc(char, strlen(transport_type) + 1);
		strcpy(n->transport_type, transport_type);
		n->configuration = new_rbtree(transport_config_parameter_key,
				transport_config_parameter_compare);
		debug(I, "Inserting new configuration for %s\n", n->transport_type);
		rb_tree_insert(transport_config, n);
	}
	parameter* p = alloc(parameter, 1);
	p->name = alloc(char, strlen(name) + 1);
	strcpy(p->name, name);
	p->value = alloc(char, strlen(value) + 1);
	strcpy(p->value, value);
	debug(I, ", with parameter %s and value %s\n", p->name, p->value);
	p = (parameter*) rb_tree_insert(n->configuration, p);
	if (p) {
		free(p->name);
		free(p->value);
		free(p);
	}
}

char* get_transport_parameter(char* transport_type, char* name) {
	transport_config_node* rt = NULL;
	if ((rt = (transport_config_node*) search_rbtree(*transport_config,
			transport_type)) == NULL) {
		debug(I, "Transport tree for %s is empty.\n", transport_type);
		return NULL;
	}debug(I, "Transport tree for %s is not empty.\n", transport_type);
	parameter *p = (parameter*) search_rbtree(*(rt->configuration), name);
	if(p)
		return p->value;
	return NULL;
}

void dump_transport_configuration() {
	tree_iterator* it = new_tree_iterator(transport_config);
	while (tree_iterator_has_next(it)) {
		transport_config_node* tc =
				(transport_config_node*) tree_iterator_next(it);
		tree_iterator* it2 = new_tree_iterator(tc->configuration);
		printf(">> %s\n", tc->transport_type);
		while (tree_iterator_has_next(it2)) {
			parameter* p = (parameter*) tree_iterator_next(it2);
			printf("\t* %s = %s\n", p->name, p->value);
		}
		destroy_iterator(it2);
	}
	destroy_iterator(it);
}
