include ../../Makefile.inc

INCLUDE_DEPTH=../../
INCL_PATH=$(INCLUDE_DEPTH)$(INCLUDE_DIR)

all: $(INCL_PATH)/registry_error.h $(INCL_PATH)/communication.h $(INCL_PATH)/registry_ops.h
	$(CC) $(CFLAGS) -I$(INCL_PATH) -c registry_ops.c
	mv registry_ops.o ../../$(BUILD_DIR)

clean:
	rm -f *.o *~