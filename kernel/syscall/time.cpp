#include <process.h>
#include <dev/RTC.h>
#include <cpu.h>

time_t sys::localtime(struct tm *tloc) {

  time_t t = dev::RTC::now();

  if (tloc != NULL) {
    if (!curproc->addr_space->validate_pointer(tloc, sizeof(*tloc), VPROT_WRITE)) {
      return -1;
    }
    dev::RTC::localtime(*tloc);
  }

  return t;
}
