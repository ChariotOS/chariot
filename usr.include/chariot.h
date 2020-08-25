#pragma once

#ifndef _CHARIOT_H
#define _CHARIOT_H


#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus


#define __NEED_ssize_t
#define __NEED_size_t
#include <bits/alltypes.h>
// placement new
inline void *operator new(size_t, void *ptr) { return ptr; }
// #include <new>

extern "C" {
#endif


#define __NEED_pid_t
#include <bits/alltypes.h>

// yield system call
int yield(void);

pid_t spawn();
pid_t despawn(int pid);



int startpidve(int pid, char *const path, char *const argv[],
               char *const envp[]);
int startpidvpe(int pid, char *const path, char *const argv[],
                char *const envp[]);
// int cmdpidv(int pid, char *const path, char *const argv[]);





#define panic(fmt, args...)                     \
  do {                                          \
    printf("PANIC: %s\n", __PRETTY_FUNCTION__); \
    printf(fmt, ##args);               \
    exit(-1);                                   \
  } while (0);

#define assert(val)                          \
  do {                                       \
    if (!(val)) {                            \
      panic("assertion failed: %s\n", #val); \
    }                                        \
  } while (0);






#ifdef __cplusplus
}
#endif

#endif
