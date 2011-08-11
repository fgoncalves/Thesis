include ../../Makefile.inc

OUT=registry_daemon
INCL_PATH=../../include/

build: registry_error.o registry.o communication.o exec.o

exec: build
	$(CC) -o $(OUT) $(CFLAGS) -I$(INCL_PATH) main.c *.o -l$(LIB_NAME)

exec.o: $(INCL_PATH)/exec.h exec.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c exec.c
	cp exec.o ../../$(BUILD_DIR)

communication.o: $(INCL_PATH)/communication.h communication.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c communication.c
	cp communication.o ../../$(BUILD_DIR)

registry.o: $(INCL_PATH)/registry.h registry.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c registry.c
	cp registry.o ../../$(BUILD_DIR)

registry_error.o: $(INCL_PATH)/registry_error.h registry_error.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c registry_error.c
	cp registry_error.o ../../$(BUILD_DIR)

install: exec
	mv $(OUT) /usr/bin/

uninstall: 
	rm -f /usr/bin/$(OUT)

clean:
	rm -f *.o *~ $(OUT)