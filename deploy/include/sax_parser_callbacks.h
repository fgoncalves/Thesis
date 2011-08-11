#ifndef __SAX_PARSER_CALLBACKS_H__
#define __SAX_PARSER_CALLBACKS_H__

#include <stdint.h>
#include "attr_list.h"

typedef int8_t (*start_element_t)(char* tag, attr_list* l, void* user_data);
typedef int8_t (*end_element_t)(char* tag, void* user_data);
typedef int8_t (*text_block_t)(char* text, uint32_t len, void* user_data);

typedef struct{
 void* user_data;
 start_element_t start;
 end_element_t end;
 text_block_t text;
}parser_cbs;

#endif
