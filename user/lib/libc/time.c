#include <time.h>
#include <sys/syscall.h>



time_t time(time_t *tloc) {
  time_t val = syscall(SYS_localtime, 0);
  if (tloc) *tloc = val;
  return val;
}

time_t getlocaltime(struct tm *tloc) {
  return syscall(SYS_localtime, tloc);
}
