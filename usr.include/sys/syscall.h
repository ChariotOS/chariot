#pragma once


#include "syscall_defs.h"


#ifdef __cplusplus
extern "C" {
#endif


// invoke a system call
extern long syscall(long number, ...);
extern long __syscall_ret(long r);

extern long errno_syscall(long number, ...);


#ifdef __cplusplus
}
#endif
