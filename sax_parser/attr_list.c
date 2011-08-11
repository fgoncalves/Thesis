#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "attr_list.h"

attr* new_attr(char* name, char* value){
  attr* new = alloc(attr, 1);

  new->name = strdup(name);
  new->value = strdup(value);
  return new;
}

void free_attr(attr* a){
  free(a->name);
  free(a->value);
  free(a);
}

attr_list* new_attr_list(){
  attr_list* al = alloc(attr_list, 1);
  return al;
}

void add_attr(attr_list* l, attr* a){
  if(!l->first){
    l->first = a;
    l->last = a;
    return;
  }

  l->last->next = a;
  l->last = a;
  return;
}

void free_attr_list(attr_list* l){
  attr* it = l->first;
  while(it){
    l->first = it->next;
    free_attr(it);
    it = l->first;
  }
  free(l);
}
