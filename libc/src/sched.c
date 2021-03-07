#include <sched.h>
#include <sys/sysbind.h>


int sched_yield(void) {
  sysbind_yield();

  return 0;
}
