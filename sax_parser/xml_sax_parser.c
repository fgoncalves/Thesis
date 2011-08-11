#include <pthread.h>
#include "xml_sax_parser.h"
#include "y.tab.h"

extern parser_cbs *__parser_callbacks;

extern void parse_file(char*);

pthread_mutex_t sax_lock = PTHREAD_MUTEX_INITIALIZER;

void parse_xml(parser_cbs cbs, char* xml) {
	pthread_mutex_lock(&sax_lock);
	__parser_callbacks = &cbs;
	parse_file(xml);
	pthread_mutex_unlock(&sax_lock);
}

void parse_xml_string(parser_cbs cbs, char* xml) {
	pthread_mutex_lock(&sax_lock);
	__parser_callbacks = &cbs;
	parse_string(xml);
	pthread_mutex_unlock(&sax_lock);
}
