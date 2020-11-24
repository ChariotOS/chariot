#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <time.h>

time_t time(time_t *tloc) {
  time_t val = sysbind_localtime(0);
  if (tloc) *tloc = val;
  return val;
}

time_t getlocaltime(struct tm *tloc) { return sysbind_localtime(tloc); }


int clock_gettime(int id, struct timespec *s) {
  long us = sysbind_gettime_microsecond();
  s->tv_nsec = us * 1000;
  s->tv_sec = us / 1000 / 1000;
  return 0;
}


clock_t clock(void) { return sysbind_gettime_microsecond() / 1000; }


// TODO: this is wrong, don't ignore t
struct tm *localtime(const time_t *t) {
  static struct tm tm;

	time_t time = getlocaltime(&tm);
	(void)time;

	// if (t != NULL) *t = time;
	return &tm;
}
