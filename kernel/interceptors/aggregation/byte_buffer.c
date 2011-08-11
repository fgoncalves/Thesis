#include <linux/slab.h>

#include "byte_buffer.h"

aggregate_buffer* create_aggregate_buffer(uint32_t len, __be32 ip, __be16 port) {
	aggregate_buffer* b = (aggregate_buffer*) kmalloc(sizeof(aggregate_buffer),
			GFP_ATOMIC);
	if (!b) {
		printk("%s:%u: Failed to alloc buffer\n", __FILE__, __LINE__);
		return NULL;
	}

	b->start = (char*) kmalloc(len, GFP_ATOMIC);

	if (!b->start) {
		printk("%s:%u: Failed to alloc buffer with size %d B\n", __FILE__,
				__LINE__, len);
		kfree(b);
		return NULL;
	}

	b->id.ip = ip;
	b->id.port = port;
	b->end = b->head = b->start;
	b->end += len;
	b->head = b->end;
	return b;
}

void free_buffer(aggregate_buffer* b) {
	kfree(b->start);
	kfree(b);
}

char* peek_packet(aggregate_buffer* b) {
	if(buffer_data_len(b) == 0)
		return NULL;
	return b->head;
}

int8_t push_bytes(aggregate_buffer* b, char* bytes, int32_t len) {
	if (buffer_data_len(b) + len <= buffer_len(b)) {
		b->head -= len;
		memcpy(b->head, bytes, len);
		return 0;
	}
	return -ENOMEM;
}
