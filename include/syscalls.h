#pragma once


enum {
#undef __SYSCALL
#define __SYSCALL(num, name) SYS_##name = num,
#include <syscalls.inc>
  SYS_COUNT
};
