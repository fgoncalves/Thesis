include ../Makefile.inc

INCLUDE_PATH=../$(INCLUDE_DIR)

HEADERS=$(wildcard $(INCLUDE_PATH)/*.h)

OBJS=rbtree.o

all: $(OBJS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -I $(INCLUDE_PATH) -c $< 
	mv *.o ../$(BUILD_DIR)	

clean:
	rm -f *~ *.out $(OBJS)