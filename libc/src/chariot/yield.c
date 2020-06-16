#include <sys/syscall.h>
#include <chariot.h>




int yield(void) {
  return syscall(SYS_yield);
}
