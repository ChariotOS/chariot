#include <stdlib.h>
#include <sys/syscall.h>
#include <stdio.h>


extern void __funcs_on_exit();

void exit(int status) {
  // call destructors and whatnot.
  __funcs_on_exit();

  syscall(SYS_exit_proc, status);
  // should not return here. Cause a segfault just in case :^)
  for (;;) {
    *(volatile char*)NULL = 0;
  }
}
