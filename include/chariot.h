#pragma once

#ifndef _CHARIOT_H
#define _CHARIOT_H


#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus


#define __NEED_ssize_t
#define __NEED_size_t
#include <bits/alltypes.h>
#include <new>
// placement new
// #include <new>

extern "C" {
#endif


#define __NEED_pid_t
#include <bits/alltypes.h>

// yield system call
int yield(void);


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
