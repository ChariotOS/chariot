#include <sys/sysbind.h>
#include <chariot.h>



int yield(void) {
  return sysbind_yield();
}
