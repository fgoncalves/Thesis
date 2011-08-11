include ../Makefile.inc

INCLUDE_PATH=../$(INCLUDE_DIR)
EXEC=config_daemon

exec: objs
	$(CC) -o $(EXEC) $(CFLAGS) *.o -l$(LIB_NAME)

objs: config.o configuration_parser.o transport_config.o configuration_socket.o configuration_socket.o

config.o: config.c
	$(CC) $(CFLAGS) -I$(INCLUDE_PATH) -c config.c
	cp *.o ../$(BUILD_DIR)

configuration_parser.o: configuration_parser.c $(INCLUDE_PATH)/configuration_parser.h
	$(CC) $(CFLAGS) -I$(INCLUDE_PATH) -c configuration_parser.c
	cp *.o ../$(BUILD_DIR)

transport_config.o: transport_config.c $(INCLUDE_PATH)/transport_config.h
	$(CC) $(CFLAGS) -I$(INCLUDE_PATH) -c transport_config.c
	cp *.o ../$(BUILD_DIR)

configuration_socket.o: configuration_socket.c $(INCLUDE_PATH)/configuration_socket.h
	$(CC) $(CFLAGS) -I$(INCLUDE_PATH) -c configuration_socket.c
	cp *.o ../$(BUILD_DIR)

install: exec
	cp $(EXEC) /usr/bin

uninstall:
	rm -f /usr/bin/$(EXEC)

clean:
	rm -f *~ *.out $(EXEC) *.o