#ifndef __STRING_BUFFER_H__
#define __STRING_BUFFRE_H__

#include <stdint.h>

#define MIN_LEN 128

typedef struct {
  uint32_t len;
  uint32_t end;
  char* buffer;
}sbuffer;

extern sbuffer* new_string_buffer();
extern void append_to_buffer(sbuffer*, char*);
extern void clean_buffer(sbuffer*);
extern void free_buffer(sbuffer*);
#endif
