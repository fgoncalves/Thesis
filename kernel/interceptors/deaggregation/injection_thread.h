#ifndef __INJECTION_THREAD_H__
#define __INJECTION_THREAD_H__

#include <linux/ip.h>

extern uint8_t start_injection_thread(void);
extern void stop_injection_thread(void);
extern void inject_packet(struct iphdr* ip);
#endif
