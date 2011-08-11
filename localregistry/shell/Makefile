include ../../Makefile.inc
INCLUDE_DIR=../../include
INC=/usr/include/$(LIB_NAME)
OUT=register

all: cmd.o argparse.o $(INC)/registry_ops.h ../user/registry_ops.c ../daemon/registry_error.o
	$(CC) -o $(OUT) -I$(INC) -I$(INCLUDE_DIR) $(CFLAGS) main.c argparse.o cmd.o -l$(LIB_NAME) ../daemon/registry_error.o

cmd.o: cmd.h cmd.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c cmd.c

argparse.o: argparse.h argparse.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c argparse.c

clean:
	rm -f $(OUT) *~ *.out *.o

install: all
	cp $(OUT) /usr/bin

uninstall:
	rm -f /user/bin/$(OUT)