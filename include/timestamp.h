#ifndef __TIMESTAMP_H__
#define __TIMESTAMP_H__

#include <time.h>
#include <stdint.h>

extern struct timespec ns_to_timespec(int64_t nsec);
extern int64_t timespec_to_ns(const struct timespec ts);
#endif
