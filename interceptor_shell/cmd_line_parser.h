#ifndef CMD_LINE_PARSER_H_
#define CMD_LINE_PARSER_H_

typedef struct {
	char *iname, *daddr, *saddr, *dport, *sport, *proto, *ruleid;
} parsed_args;

extern void print_usage(char* cmd);
extern parsed_args* parse_args(int argc, char** argv);
#endif
