/*
 * This file creates a read only proc entry which will contain the available interceptors
 * and all the rules registered. Its main purpose is for debugging issues, although it
 * can be used at any time. Why is it for debugging? First, it's an easy way to know
 * if your interceptor is correctly registered and rules are registered. Second, it doen't
 * do anything else besides listing the contents in the framework.
 *
 * Also, keep in mind that proc entries are not regular files and in this case each time
 * a read is issued, the interceptor framework needs to be scanned for registered interceptors
 * and rules. The process is simple, scan the framework, build a text representation of it in
 * memory and export it to user land. However, it can take quite a bit.
 *
 * For this reason, such text representation is cached in a buffer. If multiple reads are
 * issued to this proc entry without any changes to the framework, only the first one should
 * take a huge amount of time. If an interceptor is registered, then when the next read is
 * issued the buffer will be rebuild.
 *
 * If no read is issued the buffer will never be built. Thus in normal execution, this
 * proc entry will not place any kind of overhead.
 * */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/in.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>

#include "klist.h"
#include "proc_entry.h"
#include "interceptor.h"
#include "interceptor_manager.h"
#include "rule_manager.h"

#define PROC_FILE_NAME "interceptor_list"

struct proc_dir_entry *proc_file_entry;

typedef struct {
	//indicate if the framework has changed and this buffer should be rebuilt.
	uint8_t dirty;
	uint32_t buffer_size;
	uint32_t buffer_offset;
	char* file_buffer;
} proc_file_contents_cache;

proc_file_contents_cache proc_contents;

//If the buffer is too short for the contents of the framework,
//this function will realloc it to its double.
static void realloc_proc_contents(void) {
	char* old_buff = proc_contents.file_buffer;
	proc_contents.file_buffer = kmalloc(proc_contents.buffer_size << 1,
			GFP_ATOMIC);
	if (!proc_contents.file_buffer) {
		printk("%s in %s:%d: kmalloc failed. Unnable to create proc entry.\n",
				__FILE__, __FUNCTION__, __LINE__);
		return;
	}
	proc_contents.buffer_size <<= 1;
	proc_file_entry->size = proc_contents.buffer_size;
	kfree(old_buff);
	memset(proc_contents.file_buffer, 0, proc_contents.buffer_size);
}

//Append the given buffer to the proc entry cache.
static void append_to_buffer(char* buff_to_append, uint32_t len) {
	memcpy(proc_contents.file_buffer + proc_contents.buffer_offset, buff_to_append, len);
	proc_contents.buffer_offset += len;
}

//Fill buffer with the given filter text representation.
static void fill_buffer_with_filter(filter* f) {
	filter_specs* sp;
	char port[7], ip[17];

	if (proc_contents.buffer_size - proc_contents.buffer_offset < 68)
		realloc_proc_contents();

	switch (f->priority) {
	case FILTER_PRIO_FIRST:
		append_to_buffer("MAX\t", 4);
		break;
	case FILTER_PRIO_LAST:
		append_to_buffer("MIN\t", 4);
		break;
	case FILTER_PRIO_UNSPECIFIED:
		append_to_buffer("USP\t", 4);
		break;
	default:
		append_to_buffer("UNK\t", 4);
	}

	switch (f->filter_at) {
	case FILTER_AT_LOCAL_IN:
		append_to_buffer("Local in\t", 9);
		break;
	case FILTER_AT_LOCAL_OUT:
		append_to_buffer("Local out\t", 10);
		break;
	case FILTER_AT_FWD:
		append_to_buffer("Forward\t", 8);
		break;
	case FILTER_AT_PRE_RT:
		append_to_buffer("Pre rt\t", 7);
		break;
	case FILTER_AT_POST_RT:
		append_to_buffer("Post rt\t", 8);
		break;
	}

	sp = f->filter_specs;
	switch (sp->protocol) {
	case IPPROTO_UDP:
		append_to_buffer("UDP\t\t", 5);
		break;
	default:
		append_to_buffer("--\t\t", 4);
	}

	memset(ip, 0, 17);
	if (sp->filter_by & FILTER_BY_DST_ADDR)
#ifdef NIPQUAD
		sprintf(ip, "%d.%d.%d.%d\t", NIPQUAD(sp->dst_addr));
#else
		sprintf(ip, "%pI4\t", &(sp->dst_addr));
#endif
	else
		memcpy(ip, "--\t\t", 3);
	append_to_buffer(ip, 17);

	memset(ip, 0, 17);
	if (sp->filter_by & FILTER_BY_SRC_ADDR)
#ifdef NIPQUAD
		sprintf(ip, "%d.%d.%d.%d\t", NIPQUAD(sp->src_addr));
#else
		sprintf(ip, "%pI4\t", &(sp->src_addr));
#endif
	else
		memcpy(ip, "--\t\t", 4);
	append_to_buffer(ip, 17);

	memset(port, 0, 7);
	if (sp->filter_by & FILTER_BY_DST_PORT)
		sprintf(port, "%d\t", ntohs(sp->dst_port));
	else
		memcpy(port, "--\t", 3);
	append_to_buffer(port, 7);

	memset(port, 0, 7);
	if (sp->filter_by & FILTER_BY_SRC_PORT)
		sprintf(port, "%d", ntohs(sp->src_port));
	else
		memcpy(port, "--", 2);
	append_to_buffer(port, 7);

	append_to_buffer("\n", 1);
}

//Fill the cache contents
static void fill_proc_contents(void) {
	char* header = "List of available interceptors and registered filters:\n",
			ruleid[10] = { 0 };
	klist_iterator* i;
	int32_t filter_i;

	memset(proc_contents.file_buffer, 0, proc_contents.buffer_size);

	append_to_buffer(header, 55);

	for (i = get_interceptor_iterator(); klist_iterator_has_next(i);) {
		interceptor_descriptor* id =
				(interceptor_descriptor*) (klist_iterator_next(i))->data;
		char* name = id->name;
		if (proc_contents.buffer_size - proc_contents.buffer_offset < strlen(
				name) + 3)
			realloc_proc_contents();
		append_to_buffer("* ", 2);
		append_to_buffer(name, strlen(name));
		append_to_buffer("\n", 1);
	}
	free_klist_iterator(i);

	//rules
	for (i = get_rules_iterator(); klist_iterator_has_next(i);) {
		rule* r = (rule*) (klist_iterator_next(i))->data;
		char* name = r->interceptor->name;

		sprintf(ruleid, "%u", r->id);

		if (proc_contents.buffer_size - proc_contents.buffer_offset < strlen(
				name) + 15 + strlen(ruleid))
			realloc_proc_contents();

		append_to_buffer("\n", 1);
		append_to_buffer(">> Rule ", 8);
		append_to_buffer(ruleid, strlen(ruleid));

		memset(ruleid, 0, sizeof(ruleid));

		append_to_buffer(" for ", 5);
		append_to_buffer(name, strlen(name));
		append_to_buffer("\n", 1);

		if (proc_contents.buffer_size - proc_contents.buffer_offset < 39)
			realloc_proc_contents();

		append_to_buffer("\tPrio\tAt\tProtocol\tDst\t\tSrc\t\tDport\t\tSport\n", 41);

		//filters
		for (filter_i = 0; filter_i < r->number_of_filters; filter_i++) {
			append_to_buffer("\t", 1);
			fill_buffer_with_filter((r->filters)[filter_i]);
		}
	}
	free_klist_iterator(i);

	proc_contents.dirty = 0;
}

//Called by each read to the proc entry. If the cache is dirty it will be rebuilt.
static int procfile_read(char *buffer, char **buffer_location, off_t offset,
		int buffer_length, int *eof, void *data) {
	int how_many_can_we_cpy;

	if (proc_contents.dirty)
		fill_proc_contents();

	how_many_can_we_cpy = (proc_contents.buffer_offset + 1 - offset
			> buffer_length) ? buffer_length : proc_contents.buffer_offset + 1
			- offset;
	if (how_many_can_we_cpy == 0) {
		*eof = 1;
		return 0;
	}
	memcpy(buffer, proc_contents.file_buffer + offset, how_many_can_we_cpy);
	*buffer_location = buffer;
	return how_many_can_we_cpy;
}

static void create_proc_file(void) {
	proc_file_entry = create_proc_entry(PROC_FILE_NAME, 0644, NULL);

	if (proc_file_entry == NULL) {
		printk (KERN_ALERT "Error: Could not initialize /proc/%s\n",
				PROC_FILE_NAME);
		return;
	}

	proc_file_entry->read_proc = procfile_read;
	proc_file_entry->mode = S_IFREG | S_IRUGO;
	proc_file_entry->uid = 0;
	proc_file_entry->gid = 0;
	proc_file_entry->size = proc_contents.buffer_size;
}

static void create_proc_contents(void) {
	memset(&proc_contents, 0, sizeof(proc_file_contents_cache));
	proc_contents.dirty = 1;
	proc_contents.file_buffer = kmalloc(1024, GFP_ATOMIC);
	if (!proc_contents.file_buffer) {
		printk("%s in %s:%d: kmalloc failed. Unnable to create proc entry.\n",
				__FILE__, __FUNCTION__, __LINE__);
		return;
	}
	memset(proc_contents.file_buffer, 0, 1024);
	proc_contents.buffer_size = 1024;
}

static void clear_proc_contents(void) {
	kfree(proc_contents.file_buffer);
}

void create_interceptor_proc_entry(void) {
	create_proc_contents();
	create_proc_file();
}

void clear_interceptor_proc_entry(void) {
	clear_proc_contents();
	remove_proc_entry(PROC_FILE_NAME, NULL);
}

/*
 * This is called by the framework to notify this proc entry that the cache needs to be
 * rebuilt.
 * */
void mess_proc_entry(void) {
	proc_contents.dirty = 1;
}
