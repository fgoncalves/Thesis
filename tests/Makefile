include ../Makefile.inc

reliable_server:
	$(CC) -l$(LIB_NAME) -g -o rtserver reliable_server.c 

reliable_client:
	$(CC) -l$(LIB_NAME) -g -o rtclient reliable_client.c 

pbuffer:
	$(CC) -l$(LIB_NAME) -g -o pbuffer pbuffer.c 

handler_creation:
	$(CC) -l$(LIB_NAME) -g -o handler_creation handler_creation.c 

clean:
	rm -f *~ a.out handler_creation reliable_server reliable_client pbuffer