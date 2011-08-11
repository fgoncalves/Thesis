#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "configuration_parser.h"
#include "xml_parser.h"
#include "macros.h"

#define INTERCEPTOR_DIRECTORY_TAG "interceptor_directory"
#define INTERCEPTOR_TAG "interceptor"
#define TRANSPORT_TAG "transport"
#define PARAMETER_TAG "parameter"
#define RULE_TAG "rule"

//States
#define UNDEFINED -1
#define IN_TRANSPORT 0

int32_t current_state = UNDEFINED;
char* current_transport_type = NULL;
char* interceptor_directory = NULL;

typedef struct sh_cmd {
	char* cmd;
	struct sh_cmd* next;
} sh_cmd_t;

typedef struct {
	sh_cmd_t* first;
	sh_cmd_t* last;
} cmd_list_t;

cmd_list_t insmods = { NULL, NULL }, mkrules = { NULL, NULL };

static void add_cmd(cmd_list_t* l, char* cmd) {
	sh_cmd_t* n = alloc(sh_cmd_t, 1);

	n->cmd = alloc(char, strlen(cmd) + 1);
	strcpy(n->cmd, cmd);
	if (l->first == NULL) {
		l->first = n;
		l->last = n;
		return;
	}

	l->last->next = n;
	l->last = n;
	return;
}

static void free_cmds() {
	sh_cmd_t* aux = NULL;
	for (aux = insmods.first; aux != NULL;) {
		insmods.first = insmods.first->next;
		free(aux->cmd);
		free(aux);
		aux = insmods.first;
	}
	for (aux = mkrules.first; aux != NULL;) {
		insmods.first = insmods.first->next;
		free(aux->cmd);
		free(aux);
		aux = insmods.first;
	}
}

static void mkinsmod(attr_list* al) {
	char insmod_cmd[1000] = { 0 };
	attr* at = al->first;
	char* koname = NULL;

	strcpy(insmod_cmd, "insmod ");
	for (; at != NULL; at = at->next) {
		if (!strcmp(at->name, "module")) {
			koname = at->value;
			break;
		}
	}

	if (!koname) {
		log(E, "Unable to determine ko file to insert.");
		return;
	}

	strcat(insmod_cmd, interceptor_directory);
	strcat(insmod_cmd, koname);
	strcat(insmod_cmd, " ");

	for (; at != NULL; at = at->next) {
		if (!strcmp(at->name, "module"))
			continue;
		strcat(insmod_cmd, at->name);
		strcat(insmod_cmd, "=");
		strcat(insmod_cmd, at->value);
		strcat(insmod_cmd, " ");
	}
	debug(I, "Adding command '%s'\n", insmod_cmd);
	add_cmd(&insmods, insmod_cmd);
}

static uint8_t interceptor_directory_parser(char* tag_name, attr_list* al) {
	attr* at = al->first;
	int8_t found_id = 0;
	for (; at != NULL; at = at->next)
		if (!strcmp(at->name, "path")) {
			found_id = 1;
			break;
		}
	if (!found_id) {
		interceptor_directory = alloc(char, 3);
		strcpy(interceptor_directory, "./");
	} else {
		interceptor_directory = alloc(char, strlen(at->value) + 1);
		strcpy(interceptor_directory, at->value);
	}debug(I, "Setting module directory to %s\n", interceptor_directory);

	char cmd[100] = {0};
	strcpy(cmd, "insmod ");
	strcat(cmd, interceptor_directory);
	strcat(cmd, "interceptor_framework.ko");
	debug(I, "Issuing '%s'\n", cmd);
	system(cmd);
	return 1;
}

static uint8_t mkrule(attr_list* al) {
	//-n iname -p protocol -da destinastion_address -sa source_address -dp destination_port -sp source_port
	//<rule interceptor="synchronization" proto="UDP" daddr="192.168.0.1" saddr="192.168.0.2"	dport="57843" sport="57844" />
	attr* at = al->first;
	char cmd[1000] = { 0 };
	strcpy(cmd, "mkrule ");
	for (; at != NULL; at = at->next) {
		if (!strcmp(at->name, "interceptor")) {
			strcat(cmd, "-n ");
			strcat(cmd, at->value);
			strcat(cmd, " ");
		}
		if (!strcmp(at->name, "proto")) {
			strcat(cmd, "-p ");
			strcat(cmd, at->value);
			strcat(cmd, " ");
		}
		if (!strcmp(at->name, "daddr")) {
			strcat(cmd, "-da ");
			strcat(cmd, at->value);
			strcat(cmd, " ");
		}
		if (!strcmp(at->name, "saddr")) {
			strcat(cmd, "-sa ");
			strcat(cmd, at->value);
			strcat(cmd, " ");
		}
		if (!strcmp(at->name, "dport")) {
			strcat(cmd, "-dp ");
			strcat(cmd, at->value);
			strcat(cmd, " ");
		}
		if (!strcmp(at->name, "sport")) {
			strcat(cmd, "-sp ");
			strcat(cmd, at->value);
			strcat(cmd, " ");
		}
	}
	debug(I, "Adding command '%s'\n", cmd);
	add_cmd(&mkrules, cmd);
}

static int8_t config_start_element_cb(char* tag, attr_list* al, void* user_data) {
	if (!strcmp(tag, INTERCEPTOR_DIRECTORY_TAG)) {
		return interceptor_directory_parser(tag, al);
	}
	if (!strcmp(tag, TRANSPORT_TAG)) {
		attr* at = al->first;
		for (; at != NULL; at = at->next)
			if (!strcmp(at->name, "type")) {
				int32_t *last_state = (int32_t*) user_data;
				*last_state = IN_TRANSPORT;
				current_transport_type = alloc(char, strlen(at->value) + 1);
				strcpy(current_transport_type, at->value);
				return 1;
			}
		log(E, "Transport declared, but no type given.");
		return 0;
	}
	if (!strcmp(tag, PARAMETER_TAG)) {
		attr* at = al->first;
		char* p_name, *p_value;
		for (; at != NULL; at = at->next) {
			if (!strcmp(at->name, "name"))
				p_name = at->value;
			if (!strcmp(at->name, "value"))
				p_value = at->value;
		}
		int32_t *last_state = (int32_t*) user_data;
		switch (*last_state) {
		case IN_TRANSPORT:
			set_transport_parameter(current_transport_type, p_name, p_value);
			return 1;
		}
		log(E, "Unable to determine where parameter should be placed.");
		return 0;
	}
	if (!strcmp(tag, INTERCEPTOR_TAG)) {
		mkinsmod(al);
		return 1;
	}
	if (!strcmp(tag, RULE_TAG)) {
		mkrule(al);
		return 1;
	}
}

static int8_t config_end_element_cb(char* tag, void* user_data) {
	if (!strcmp(tag, TRANSPORT_TAG)) {
		int32_t *last_state = (int32_t*) user_data;
		*last_state = UNDEFINED;
		free(current_transport_type);
	}
	return 1;
}

static int8_t config_text_block_element_cb(char* text, uint32_t len,
		void* user_data) {
	return 1;
}

static void execute_cmds() {
	sh_cmd_t* cmd;
	for (cmd = insmods.first; cmd != NULL; cmd = cmd->next) {
		debug(I, "Issuing '%s'\n", cmd->cmd);
		system(cmd->cmd);
	}
	for (cmd = mkrules.first; cmd != NULL; cmd = cmd->next) {
		debug(I, "Issuing '%s'\n", cmd->cmd);
		system(cmd->cmd);
	}
}

uint8_t parse_configuration_file(char* file) {
	parser_cbs cbs = { .user_data = &current_state, .start =
			config_start_element_cb, .end = config_end_element_cb, .text =
			config_text_block_element_cb };
	parse_xml(cbs, file);
	execute_cmds();
}

void configuration_cleanup() {
	free(interceptor_directory);
	free_cmds();
	free_transport_config();
}
