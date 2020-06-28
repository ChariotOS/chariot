#include <time.h>
#include <sys/syscall.h>


static inline size_t current_us() { return syscall(SYS_gettime_microsecond); }

time_t time(time_t *tloc) {
  time_t val = syscall(SYS_localtime, 0);
  if (tloc) *tloc = val;
  return val;
}

time_t getlocaltime(struct tm *tloc) {
  return syscall(SYS_localtime, tloc);
}


int clock_gettime(int id, struct timespec *s) {
	long us = current_us();
	s->tv_nsec = us * 1000;
	s->tv_sec = us / 1000 / 1000;
	return 0;
}



clock_t clock(void) {
	return current_us() / 1000;
}
