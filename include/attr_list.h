#ifndef __ATTR_LIST_H__
#define __ATTR_LIST_H__

typedef struct attr_node{
  char* name;
  char* value;
  struct attr_node* next;
}attr;

typedef struct sattr_list{
  attr* first;
  attr* last;
}attr_list;

extern attr* new_attr(char* name, char* value);
extern void free_attr(attr* a);
extern attr_list* new_attr_list();
extern void add_attr(attr_list*, attr* a);
extern void free_attr_list(attr_list*);
#endif
