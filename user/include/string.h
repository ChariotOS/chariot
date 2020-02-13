#pragma once

#ifndef _STRING_H
#define _STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#include <bits/alltypes.h>

void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
void *memchr(const void *, int, size_t);

char *strcpy(char *__restrict, const char *__restrict);
char *strncpy(char *__restrict, const char *__restrict, size_t);


size_t strlen(const char *s);


char *strchr(const char *s, int c);


int strcmp(const char *l, const char *r);


#ifdef __cplusplus
}
#endif

#endif
