#ifndef __CONNECTION_TABLE_H__
#define __CONNECTION_TABLE_H__

#include "connection.h"
#include "rbtree.h"

typedef tree_root connection_table;

extern connection_table* mkconnection_table();
extern void free_connection_table(connection_table* ct);

/*
 * This function shall lookup a connection in the given table. If such connection does not exist,
 * it will be created.
 * */
extern rt_connection* get_connection(connection_table* ct, uint16_t id, uint32_t window);
extern void remove_connection(connection_table* ct, uint16_t id);
#endif
