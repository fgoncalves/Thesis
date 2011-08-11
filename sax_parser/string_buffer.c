#include <string.h>
#include <stdlib.h>

#include "string_buffer.h"
#include "macros.h"

sbuffer* new_string_buffer(){
  sbuffer* new = alloc(sbuffer, 1);
  new->buffer = alloc(char, MIN_LEN);
  new->end = 0;
  new->len = MIN_LEN;
  return new;
}

static void reallocate_buffer(sbuffer* s){
  s->buffer = (char*) realloc(s->buffer, s->len << 1);

  if(!s->buffer){
    log(F, "Failed to realloc string buffer.\n");
    exit(-1);
  }
  
  memset(&(s->buffer[s->end]), 0, s->len - s->end);
  s->len <<= 1;
}

void append_to_buffer(sbuffer* s, char* str){
  uint32_t str_len = strlen(str);

  while(s->len - 1 - s->end - str_len < 0) //Account for '\0'
    reallocate_buffer(s);

  if(s->end != 0)
    s->buffer[s->end++] = ' ';
  
  memcpy(&(s->buffer[s->end]), str, str_len);
  s->end += str_len;
}

void clean_buffer(sbuffer* s){
  if(s->len == MIN_LEN){
    memset(s->buffer, 0, MIN_LEN);
    s->end = 0;
    return;
  }

  free(s->buffer);
  s->buffer = alloc(char, MIN_LEN);
  s->end = 0;
  s->len = MIN_LEN;
}

void free_buffer(sbuffer* s){
  free(s->buffer);
  free(s);
}
