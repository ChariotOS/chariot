#pragma once
#include <sys/cdefs.h>
#include <sys/types.h>
#include <time.h>

__BEGIN_DECLS

struct timeval {
  time_t tv_sec;
  suseconds_t tv_usec;
};


struct timezone {
  int tz_minuteswest;
  int tz_dsttime;
};



int gettimeofday(struct timeval*, void*);

__END_DECLS
