#include "timestamp.h"

#define nsec_per_sec    1000000000L

static long div_ulong_rem(int64_t dividend, int divisor, int *remainder){
  *remainder = dividend % divisor;
  return dividend / divisor;
}

struct timespec ns_to_timespec(int64_t nsec){
  struct timespec ts;
  int rem;
  
  if (!nsec)
    return (struct timespec) {0, 0};
  
  ts.tv_sec = div_ulong_rem(nsec, nsec_per_sec, &rem);
  ts.tv_nsec = rem;
  
  return ts;
}

int64_t timespec_to_ns(const struct timespec ts){
  return ((int64_t) ts.tv_sec * (int64_t) nsec_per_sec) + (int64_t) ts.tv_nsec;
}
