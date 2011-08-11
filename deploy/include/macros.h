#ifndef __MACROS_H__
#define __MACROS_H__

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef DEBUG
#define debug(level, format, ...)					\
  do{									\
    fprintf(stderr, "%s  %s:%d in %s: ", level, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    fprintf(stderr, format, ## __VA_ARGS__);				\
  }while(0)
#else
#define debug(level, str, ...)
#endif

#define log(level, format, ...)						\
  do{									\
    fprintf(stderr, "%s  %s:%d in %s: ", level, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    fprintf(stderr, format, ## __VA_ARGS__);				\
  }while(0)

#define I "INFO"
#define W "WARNNING"
#define E "ERROR"
#define F "FATAL ERROR"

#define alloc(type, how_many)						\
  (type *) __alloc(malloc(how_many*sizeof(type)), how_many*sizeof(type));

#define alloc_bytes(type, how_many_bytes)			\
  (type *) __alloc(malloc(how_many_bytes), (how_many_bytes));

static inline void* __alloc(void* x, uint32_t len){
  if(x){
    memset(x, 0, len);
    return x;
  }
  log(F,"calloc failed.");
  exit(-1);
  return 0;
}

#define true 1
#define false 0

#endif
