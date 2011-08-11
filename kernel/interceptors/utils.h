#ifndef __UTILS_H__
#define __UTILS_H__

#define SEC_2_NSEC 1000000000L
#define USEC_2_NSEC 1000L

#define print_error(format, ...)\
	do{\
		printk(KERN_EMERG "%s - %s:%d: ", __FUNCTION__, __FILE__, __LINE__);\
		printk(format, ## __VA_ARGS__);\
	}while(0)

static inline s64 get_kernel_current_time(void) {
	struct timeval t;
	memset(&t, 0, sizeof(struct timeval));
	do_gettimeofday(&t);
	return ((int64_t) t.tv_sec) * SEC_2_NSEC + ((int64_t) t.tv_usec)
			* USEC_2_NSEC;
}

static inline s64 swap_time_byte_order(s64 time) {
	unsigned char bytes[8];

	bytes[0] = ((char*) &time)[7];
	bytes[1] = ((char*) &time)[6];
	bytes[2] = ((char*) &time)[5];
	bytes[3] = ((char*) &time)[4];
	bytes[4] = ((char*) &time)[3];
	bytes[5] = ((char*) &time)[2];
	bytes[6] = ((char*) &time)[1];
	bytes[7] = ((char*) &time)[0];
	return *((s64*) bytes);
}

static inline uint16_t csum(uint16_t* buff, int nwords) {
	uint32_t sum;
	for (sum = 0; nwords > 0; nwords--)
		sum += *buff++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return ((uint16_t) ~sum);
}

static inline __tp(pdu)* pdu_skb(struct sk_buff* skb) {
	struct iphdr* iph = ip_hdr(skb);

	if(iph->protocol != IPPROTO_UDP) {
		printk("%s in %s:%d: Cannot access pdu. IP protocol is not UDP.\n", __FUNCTION__, __FILE__, __LINE__);
		return NULL;
	}

	return (__tp(pdu)*) (((char*) iph) + (iph->ihl << 2) + sizeof(struct udphdr) + sizeof(control_byte));
}

static inline control_byte* control_byte_skb(struct sk_buff* skb) {
	struct iphdr* iph = ip_hdr(skb);

	if (iph->protocol != IPPROTO_UDP) {
		printk(
				"%s in %s:%d: Cannot access control byte. IP protocol is not UDP.\n",
				__FUNCTION__, __FILE__, __LINE__);
		return NULL;
	}

	return (control_byte*) (((char*) iph) + (iph->ihl << 2)
			+ sizeof(struct udphdr));
}

static inline void dump_ip_skb(struct sk_buff *skb) {
	struct iphdr* iph = ip_hdr(skb);
	int32_t i;
	for (i = 0; i < ntohs(iph->tot_len); i++)
		printk("%02X ", ((uint8_t*) iph)[i]);
	printk("\n");
}

#define n_pdu_len(P)\
	(ntohs((P)->len) + sizeof(__tp(pdu)))
#endif
