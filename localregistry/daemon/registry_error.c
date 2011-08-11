#include <stdlib.h>
#include <string.h>
#include "communication_types.h"
#include "registry_error.h"

char* registry_error_str(uint32_t errorcode){
  switch(errorcode){
  case H_ERROR_DUPLICATE_ID:
    return "There is a handler already registered with the given id.";
  case H_ERROR_VMALLOC_FAIL:
    return "Vmalloc failed to allocate memory for handler.";
  case H_ERROR_NOT_INITIALIZED:
    return "Handler registry is not initialized.";
  case H_ERROR_NO_HANDLER:
    return "There is no handler with the provided id.";
  case HERROR_UNKNOWN_REQ:
    return "Unknown request.";
  default:
    return strerror(errorcode);
  }
  return NULL;
}
