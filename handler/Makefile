include ../Makefile.inc

INCL_PATH=../$(INCLUDE_DIR)

HEADERS=$(wildcard $(INCLUDE_PATH)/*.h)

OBJS=message_buffer.o sockiface.o unreliable_transport.o handler.o timestamp.o reliable_transport.o connection_table.o connection.o packet_buffer.o recv_buff.o

all: $(OBJS)
	mv $(OBJS) ../$(BUILD_DIR)
	
sockiface.o: $(INCL_PATH)/sockiface.h sockiface.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c sockiface.c

message_buffer.o: $(INCL_PATH)/message_buffer.h message_buffer.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c message_buffer.c

unreliable_transport.o: $(INCL_PATH)/unreliable_transport.h unreliable_transport.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c unreliable_transport.c

reliable_transport.o: $(INCL_PATH)/reliable_transport.h reliable_transport/reliable_transport.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c reliable_transport/reliable_transport.c
	
connection_table.o: $(INCL_PATH)/connection_table.h reliable_transport/connection_table.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c reliable_transport/connection_table.c

connection.o: $(INCL_PATH)/connection.h reliable_transport/connection.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c reliable_transport/connection.c

packet_buffer.o: $(INCL_PATH)/packet_buffer.h reliable_transport/packet_buffer.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c reliable_transport/packet_buffer.c

recv_buff.o: $(INCL_PATH)/recv_buff.h reliable_transport/recv_buff.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c reliable_transport/recv_buff.c

handler.o: $(INCL_PATH)/handler.h handler.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c handler.c

timestamp.o: $(INCL_PATH)/timestamp.h timestamp.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c timestamp.c

clean:
	rm -f *~ *.out $(OBJS)