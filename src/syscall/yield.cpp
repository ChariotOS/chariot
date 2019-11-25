#include <process.h>


int sys::yield(void) {
  sched::yield();
  return 0;
}

