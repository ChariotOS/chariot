#include <stdlib.h>
#include <sys/syscall.h>


void exit(int status) {
  syscall(SYS_exit_proc, status);
  // should not return here. Cause a segfault just in case
  *(volatile char*)NULL = 0;
  for (;;) {}
}
