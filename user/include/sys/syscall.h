#pragma once


// invoke a system call
long syscall(long number, ...);
long __syscall_ret(unsigned long r);


#define SYS_restart 0
#define SYS_exit 1
#define SYS_open 2
#define SYS_close 3
#define SYS_lseek 4
#define SYS_read 5
#define SYS_write 6
#define SYS_yield 7

#define SYS_pctl 8

#define SYS_dup 9
#define SYS_dup2 10

#define SYS_stat 11
#define SYS_fstat 12
#define SYS_lstat 13

#define SYS_mmap 60
#define SYS_munmap 61

