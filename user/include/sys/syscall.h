#pragma once


#include "syscall_defs.h"

// invoke a system call
long syscall(long number, ...);
long __syscall_ret(unsigned long r);

long errno_syscall(long number, ...);
