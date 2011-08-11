include ../../Makefile.inc

INCLUDE_PATH=../../$(INCLUDE_DIR)
INCLUDE_DAEMON_PATH=../

config.o: configuration_user_communication.c
	$(CC) $(CFLAGS) -I$(INCLUDE_PATH) -I$(INCLUDE_DAEMON_PATH) -c configuration_user_communication.c
	mv *.o ../../$(BUILD_DIR)
	
clean:
	rm -f *~ *.out *.o