#pragma once


#include "syscall_defs.h"
#include <errno.h>


#ifdef __cplusplus
extern "C" {
#endif


// invoke a system call
extern long syscall(long number, ...);
extern long __syscall_ret(long r);

extern long errno_syscall(long number, ...);

static inline long errno_wrap(long res) {
  if (res < 0) {
    errno = -res;
    return -1;
  }
  errno = 0;
  return res;
}


#ifdef __cplusplus
}
#endif
