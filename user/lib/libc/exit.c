#include <stdlib.h>
#include <sys/syscall.h>


void exit(int status) {

  // TODO: call destructors and whatnot.

  syscall(SYS_exit_proc, status);
  // should not return here. Cause a segfault just in case :^)
  for (;;) {
    *(volatile char*)NULL = 0;
  }
}



