#pragma once

#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif


#undef NULL

#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void *)0)
#endif

#define __NEED_size_t
#include <bits/alltypes.h>

void *malloc(size_t sz);
void free(void *);

void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);


void exit(int status);

#ifdef __cplusplus
}
#endif

#endif
