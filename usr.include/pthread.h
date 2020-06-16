#pragma once

#ifndef __PTHREAD_H
#define __PTHREAD_H


#define __NEED_size_t
#include <bits/alltypes.h>


/* do not use -- use pthread_t instead */
struct __pthread {
  int tid;
};

typedef struct __pthread pthread_t;


/* TODO: ??? */
typedef struct { union { int __i[9]; size_t __s[9]; } __u; } pthread_attr_t;


int pthread_create(pthread_t *, const pthread_attr_t *, void *(*fn)(void *), void *arg);

#endif /* __PTHREAD_H */
