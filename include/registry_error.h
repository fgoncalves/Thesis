#ifndef __REGISTRY_ERROR_H__
#define __REGISTRY_ERROR_H__

#include <stdint.h>

enum{
  H_ERROR_DUPLICATE_ID = 1024,
  H_ERROR_VMALLOC_FAIL,
  H_ERROR_NOT_INITIALIZED,
  H_ERROR_NO_HANDLER,
};

extern char* registry_error_str(uint32_t errorcode);
#endif
