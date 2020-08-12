#include <time.h>
#include <sys/syscall.h>
#include <sys/sysbind.h>

time_t time(time_t *tloc) {
  time_t val = sysbind_localtime(0);
  if (tloc) *tloc = val;
  return val;
}

time_t getlocaltime(struct tm *tloc) {
  return sysbind_localtime(tloc);
}


int clock_gettime(int id, struct timespec *s) {
	long us = sysbind_gettime_microsecond();
	s->tv_nsec = us * 1000;
	s->tv_sec = us / 1000 / 1000;
	return 0;
}



clock_t clock(void) {
	return sysbind_gettime_microsecond() / 1000;
}
