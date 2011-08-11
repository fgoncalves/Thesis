#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <fred/macros.h>
#include <fred/message_buffer.h>
#include <fred/packet_buffer.h>

int main() {
	packet_buffer *b = mkpacket_buffer(5, 123);
	int i;
	msg* m;
	for (i = 0; i < 5; i++) {
		m = alloc(msg, 1);

		m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
		m->dst_app_id = i;
		m->pdu->id = 123;
		m->pdu->seq = i;
		add_packet(b, m);
	}

	assert((get_next_packet_to_send(b))->pdu->seq == 0);
	assert((get_next_packet_to_send(b))->pdu->seq == 1);
	assert((get_next_packet_to_send(b))->pdu->seq == 2);
	assert((get_next_packet_to_send(b))->pdu->seq == 3);
	assert((get_next_packet_to_send(b))->pdu->seq == 4);
	assert(get_next_packet_to_send(b) == NULL);

	assert(((b->buffer)[b->oldest])->pdu->seq == 0);
	assert(((b->buffer)[b->latest]) == NULL);

	m = alloc(msg, 1);

	m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
	m->dst_app_id = 5;
	m->pdu->seq = 5;
	m->pdu->id = 123;
	add_packet(b, m);

	assert(((b->buffer)[b->oldest])->pdu->seq == 1);
	assert((get_next_packet_to_send(b))->pdu->seq == 5);

	free_packet_buffer(b);

	b = mkpacket_buffer(5, 123);
	for (i = 0; i < 5; i++) {
		m = alloc(msg, 1);

		m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
		m->dst_app_id = i;
		m->pdu->id = 123;
		m->pdu->seq = i;
		add_packet(b, m);
		assert((get_next_packet_to_send(b))->pdu->seq == i);
	}assert((get_next_packet_to_send(b)) == NULL);
	assert(((b->buffer)[b->oldest])->pdu->seq == 0);
	free_packet_buffer(b);

	b = mkpacket_buffer(5, 123);
	for (i = 0; i < 3; i++) {
		m = alloc(msg, 1);

		m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
		m->dst_app_id = i;
		m->pdu->id = 123;
		m->pdu->seq = i;
		add_packet(b, m);
	}assert((get_next_packet_to_send(b))->pdu->seq == 0);
	assert((get_next_packet_to_send(b))->pdu->seq == 1);
	assert((get_next_packet_to_send(b))->pdu->seq == 2);
	assert((get_next_packet_to_send(b)) == NULL);
	assert(((b->buffer)[b->oldest])->pdu->seq == 0);
	for (i = 3; i < 5; i++) {
		m = alloc(msg, 1);

		m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
		m->dst_app_id = i;
		m->pdu->id = 123;
		m->pdu->seq = i;
		add_packet(b, m);
	}assert((get_next_packet_to_send(b))->pdu->seq == 3);
	assert((get_next_packet_to_send(b))->pdu->seq == 4);
	assert((get_next_packet_to_send(b)) == NULL);
	assert(((b->buffer)[b->oldest])->pdu->seq == 0);
	m = alloc(msg, 1);

	m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
	m->dst_app_id = 5;
	m->pdu->id = 123;
	m->pdu->seq = 5;
	add_packet(b, m);
	assert((get_next_packet_to_send(b))->pdu->seq == 5);
	assert((get_next_packet_to_send(b)) == NULL);
	assert(((b->buffer)[b->oldest])->pdu->seq == 1);
	free_packet_buffer(b);

	//Test buffer overflow
	b = mkpacket_buffer(5, 123);

	for (i = 0; i < 50; i++) {
		m = alloc(msg, 1);

		m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
		m->dst_app_id = i;
		m->pdu->id = 123;
		m->pdu->seq = i;
		add_packet(b, m);
		assert((get_next_packet_to_send(b))->pdu->seq == i);
	}assert((get_next_packet_to_send(b)) == NULL);
	free_packet_buffer(b);

	b = mkpacket_buffer(5, 123);
	//Test set_buffer_pointer
	for (i = 0; i < 5; i++) {
		m = alloc(msg, 1);

		m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
		m->dst_app_id = i;
		m->pdu->id = 123;
		m->pdu->seq = i;
		add_packet(b, m);
	}

	assert((get_next_packet_to_send(b))->pdu->seq == 0);
	assert((get_next_packet_to_send(b))->pdu->seq == 1);
	assert((get_next_packet_to_send(b))->pdu->seq == 2);
	assert((get_next_packet_to_send(b))->pdu->seq == 3);
	assert((get_next_packet_to_send(b))->pdu->seq == 4);
	assert(get_next_packet_to_send(b) == NULL);

	set_buffer_pointer_to(b, 0); //Set it to the oldest packet available
	assert((get_next_packet_to_send(b))->pdu->seq == 0);
	assert((get_next_packet_to_send(b))->pdu->seq == 1);
	assert((get_next_packet_to_send(b))->pdu->seq == 2);
	assert((get_next_packet_to_send(b))->pdu->seq == 3);
	assert((get_next_packet_to_send(b))->pdu->seq == 4);
	assert(get_next_packet_to_send(b) == NULL);

	set_buffer_pointer_to(b, 3);
	assert((get_next_packet_to_send(b))->pdu->seq == 3);
	assert((get_next_packet_to_send(b))->pdu->seq == 4);
	assert(get_next_packet_to_send(b) == NULL);

	set_buffer_pointer_to(b, 4);
	assert((get_next_packet_to_send(b))->pdu->seq == 4);
	assert(get_next_packet_to_send(b) == NULL);

	m = alloc(msg, 1);
	m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
	m->dst_app_id = 5;
	m->pdu->id = 123;
	m->pdu->seq = 5;
	add_packet(b, m);
	set_buffer_pointer_to(b, 0); //Set it to a packet no longer available
	assert((get_next_packet_to_send(b))->pdu->seq == 1);
	assert((get_next_packet_to_send(b))->pdu->seq == 2);
	assert((get_next_packet_to_send(b))->pdu->seq == 3);
	assert((get_next_packet_to_send(b))->pdu->seq == 4);
	m = alloc(msg, 1);
	m->pdu = alloc_bytes(__tp(pdu), sizeof(__tp(pdu)));
	m->dst_app_id = 6;
	m->pdu->id = 123;
	m->pdu->seq = 6;
	add_packet(b, m);
	set_buffer_pointer_to(b, 0); //Set it to a packet no longer available
	assert((get_next_packet_to_send(b))->pdu->seq == 2);
	assert((get_next_packet_to_send(b))->pdu->seq == 3);
	assert((get_next_packet_to_send(b))->pdu->seq == 4);
	assert((get_next_packet_to_send(b))->pdu->seq == 5);
	assert((get_next_packet_to_send(b))->pdu->seq == 6);
	assert(get_next_packet_to_send(b) == NULL);

	printf("pass\n");
	free_packet_buffer(b);
	return 0;
}
