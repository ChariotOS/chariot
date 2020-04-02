#include <cpu.h>
#include <dev/RTC.h>
#include <syscall.h>
#include <time.h>

time_t sys::localtime(struct tm *tloc) {
  time_t t = time::now_ms() / 1000;

  if (tloc != NULL) {
    if (!curproc->mm->validate_pointer(tloc, sizeof(*tloc), VPROT_WRITE)) {
      return -1;
    }
    dev::RTC::localtime(*tloc);
  }

  return t;
}

size_t sys::gettime_microsecond(void) {
	return time::now_us();
}
