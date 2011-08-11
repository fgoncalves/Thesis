#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/delay.h>

#include "pdu.h"
#include "utils.h"
#include "spin_lock_queue.h"
#include "injection_thread.h"

static sl_queue_t* queue = NULL;
static struct task_struct * injector = NULL;
static struct socket * udp_socket = NULL;

static void send_it(struct iphdr* ip) {
	struct msghdr msg;
	mm_segment_t oldfs;
	struct iovec iov;
	struct sockaddr_in addr;
	struct udphdr *udp;
	int status;

	udp = (struct udphdr*) (((char*) ip) + (ip->ihl << 2));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ip->daddr;
	addr.sin_port = udp->dest;

	udp->check = 0;

	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(struct sockaddr_in);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = MSG_NOSIGNAL;

	iov.iov_base = ip;
	iov.iov_len = (__kernel_size_t) ntohs(ip->tot_len);

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	if((status = sock_sendmsg(udp_socket, &msg, (size_t) ntohs(ip->tot_len))) < 0){
		print_error(KERN_EMERG "FAILED TO SEND MESSAGE THROUGH SOCKET. ERROR %d\n", -status);
	}
	set_fs(oldfs);
}

static int main_loop(void* data) {
	struct iphdr* packet = NULL;
	char __user one = 1;
	char __user *val = &one;

	if (sock_create(AF_INET, SOCK_RAW, IPPROTO_UDP, &udp_socket) < 0) {
		print_error(KERN_EMERG "Unable to create socket.\n");
		return 0;
	}

	if (udp_socket->ops->setsockopt(udp_socket, IPPROTO_IP, IP_HDRINCL, val,
			sizeof(val)) < 0) {
		print_error(KERN_EMERG "Unable to set socket option.\n");
		return 0;
	}

	while (1) {
		if (kthread_should_stop())
			break;
		packet = (struct iphdr*) spin_lock_queue_dequeue(queue);
		if (packet) {
			send_it(packet);
			kfree(packet);
		}
		msleep(100);
	}
	return 0;
}

uint8_t start_injection_thread(void) {
	queue = make_spin_lock_queue();
	if (!queue)
		return 0;

	injector = kthread_run(main_loop, NULL, "injector-thread");
	if (IS_ERR(injector)) {
		print_error("Unable to create aggregation injection thread.\n");
		kfree(injector);
		injector = NULL;
		return 0;
	}
	return 1;
}

void stop_injection_thread(void) {
	if (injector) {
		free_spin_lock_queue(queue, NULL);
		kthread_stop(injector);
	}
}

void inject_packet(struct iphdr* ip) {
	struct iphdr* new = kmalloc(ntohs(ip->tot_len), GFP_ATOMIC);
	if (!new) {
		print_error("Unable to allocate packet to inject.");
		return;
	}
	memcpy(new, ip, ntohs(ip->tot_len));
	spin_lock_queue_enqueue(queue, new);
}
