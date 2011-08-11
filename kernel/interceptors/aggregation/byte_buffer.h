#ifndef __BYTE_BUFFER_H__
#define __BYTE_BUFFER_H__

#include <linux/kernel.h>

/* |------------------------------------|
 * |                  |  2      |   1   |
 * |------------------------------------|
 * ^-start            ^-head            ^-end
 */

typedef struct aggbuff {
	struct{
		__be32 ip;
		__be16 port;
	}id;
	char* start;
	char* end;
	char* head;
} aggregate_buffer;

#define reset_buffer(B)				\
  ((B)->head = (B)->end)

#define buffer_len(B)				\
  ((B)->end - (B)->start)

#define buffer_data_len(B)			\
  ((B)->end - (B)->head)

extern aggregate_buffer* create_aggregate_buffer(uint32_t len, __be32 ip, __be16 port);
extern void free_buffer(aggregate_buffer* b);
extern char* peek_packet(aggregate_buffer* b);
extern int8_t push_bytes(aggregate_buffer* b, char* bytes, int32_t len);
#endif
