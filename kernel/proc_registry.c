/*
 * This file implements a write only proc entry which is used to register/unregister rules for
 * with the rule manager.
 *
 * The idea is to write a string in the form
 * '<R>:<interceptor name>:<ip proto>:<dest address>:<source address>:<dest port>:<source port>'
 * or
 * '<U>:<rule id>'
 *
 * Where R|U means: R - register, U - unregister.
 *
 * I used a proc entry to communicate between user land and kernel land because for this
 * case it's the simplest way of doing it.
 *
 * Scripts such as mkrule and rmrule create the above strings and write them to this file.
 * */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/in.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>

#include "filter.h"
#include "proc_registry.h"
#include "interceptor.h"
#include "interceptor_manager.h"

#define PROC_REGISTRY_FILE_NAME "interceptor_registry"
#define MAX_REGISTRY_LINE 800 //characters
struct proc_dir_entry *proc_registry_file_entry;

static void parse_request(char* req, int len) {
	char *start, *end;
	char *interceptor_name = NULL, proto_s[4] = { 0 }, addr_s[16] = { 0 },
			port_s[6] = { 0 }, ruleid_s[20] = {0};
	uint8_t ipproto = 0, filter_by = 0;
	uint32_t ruleid;
	__be32 daddr = 0, saddr = 0;
	__be16 dport = 0, sport = 0;
	filter_specs* sp;

	switch (req[0]) {
	case 'R':
		for (start = end = req + 2; *end != ':' && *end != '\0'; end++)
			;

		if (start == end) {
			printk("Unable to parse interceptor name for proc registry.\n");
			return;
		}

		interceptor_name = kmalloc(end - start + 1, GFP_ATOMIC);
		if (!interceptor_name) {
			printk(
					"%s in %s:%d: kmalloc failed. Unnable to create name for interceptor.\n",
					__FILE__, __FUNCTION__, __LINE__);
			return;
		}
		memset(interceptor_name, 0, end - start + 1);
		memcpy(interceptor_name, start, end - start);

		//protocol
		for (start = end = end + 1; *end != ':' && *end != '\0'; end++)
			;

		if (start != end) {
			memcpy(proto_s, start, end - start);
			ipproto = simple_strtol(proto_s, NULL, 10);
			filter_by |= FILTER_BY_L4_PROTO;
		}

		//destination address
		for (start = end = end + 1; *end != ':' && *end != '\0'; end++)
			;

		if (start != end) {
			memcpy(addr_s, start, end - start);
			daddr = simple_strtol(addr_s, NULL, 10);
			memset(addr_s, 0, 16);
			filter_by |= FILTER_BY_DST_ADDR;
		}

		//source address
		for (start = end = end + 1; *end != ':' && *end != '\0'; end++)
			;

		if (start != end) {
			memcpy(addr_s, start, end - start);
			saddr = simple_strtol(addr_s, NULL, 10);
			filter_by |= FILTER_BY_SRC_ADDR;
		}

		//destination port
		for (start = end = end + 1; *end != ':' && *end != '\0'; end++)
			;

		if (start != end) {
			memcpy(port_s, start, end - start);
			dport = simple_strtol(port_s, NULL, 10);
			memset(port_s, 0, 7);
			filter_by |= FILTER_BY_DST_PORT;
		}

		//source port
		for (start = end = end + 1; *end != ':' && *end != '\0'; end++)
			;

		if (start != end) {
			memcpy(port_s, start, end - start);
			sport = simple_strtol(port_s, NULL, 10);
			filter_by |= FILTER_BY_SRC_PORT;
		}

		sp = create_filter_specs(filter_by, daddr, saddr, dport, sport, ipproto);

		if (!create_rule(interceptor_name, sp))
			printk("Proc registry could not create rule. Create rule failed.\n");
		break;
	case 'U':
		for (start = end = req + 2; *end != ':' && *end != '\0'; end++)
			;
		if(start == end){
			printk("Proc registry could not get rule id.\n");
			break;
		}

		memcpy(ruleid_s, start, end - start);
		ruleid = simple_strtol(ruleid_s, NULL, 10);

		remove_rule(ruleid);
		break;
	default:
		printk("Unknown request to proc registry.\n");
	}
	return;
}

static int procfile_write(struct file *file, const char *buffer,
		unsigned long count, void *data) {
	char internal_buffer[MAX_REGISTRY_LINE] = { 0 };

	if (count > MAX_REGISTRY_LINE) {
		return -ENOMEM;
	}

	if (copy_from_user(internal_buffer, buffer, count)) {
		return -EFAULT;
	}

	parse_request(internal_buffer, count);

	return count;
}

static void create_proc_file(void) {
	proc_registry_file_entry = create_proc_entry(PROC_REGISTRY_FILE_NAME, 0644,
			NULL);

	if (proc_registry_file_entry == NULL) {
		printk (KERN_ALERT "Error: Could not initialize /proc/%s\n",
				PROC_REGISTRY_FILE_NAME);
		return;
	}

	proc_registry_file_entry->write_proc = procfile_write;
	proc_registry_file_entry->mode = S_IFREG | S_IRUGO | S_IWUSR | S_IWGRP
			| S_IWOTH;
	proc_registry_file_entry->uid = 0;
	proc_registry_file_entry->gid = 0;
}

void create_proc_registry(void) {
	create_proc_file();
}

void remove_proc_registry(void) {
	remove_proc_entry(PROC_REGISTRY_FILE_NAME, NULL);
}
