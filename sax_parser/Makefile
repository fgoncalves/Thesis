include ../Makefile.inc

INCLUDE_DEPTH=../
INCL_PATH=$(INCLUDE_DEPTH)$(INCLUDE_DIR)

all: parser attr_list.o string_buffer.o xml_sax_parser.o
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c lex.yy.c y.tab.c
	mv lex.yy.o y.tab.o ../$(BUILD_DIR)

xml_sax_parser.o: $(INCL_PATH)/xml_sax_parser.h xml_sax_parser.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c xml_sax_parser.c
	mv xml_sax_parser.o ../$(BUILD_DIR)

string_buffer.o: $(INCL_PATH)/string_buffer.h string_buffer.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c string_buffer.c
	mv string_buffer.o ../$(BUILD_DIR)

attr_list.o: $(INCL_PATH)/attr_list.h attr_list.c
	$(CC) $(CFLAGS) -I $(INCL_PATH) -c attr_list.c
	mv attr_list.o ../$(BUILD_DIR)

parser: sax_parser.y sax_parser.l $(INCL_PATH)/sax_parser_callbacks.h
	yacc -d sax_parser.y
	lex sax_parser.l
	mv y.tab.h ../include 

clean:
	rm -f *.o ../include/y.tab.h lex.yy.c *~