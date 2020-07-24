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

char *strcat(char *dest, const char *src);

size_t strlen(const char *s);

char *strchr(const char *s, int c);
int strcmp(const char *l, const char *r);
int strncmp(const char *s1, const char *s2, size_t n);

// I dont have locales
int strcoll(const char *s1, const char *s2);

size_t strspn(const char *s, const char *c);
size_t strcspn(const char *s, const char *c);

char *strtok_r(char *str, const char *delim, char **saveptr);
char *strtok(char *str, const char *delim);

char *strdup(const char *s);
char *stpcpy(char *d, const char *s);
char *strcpy(char *dest, const char *src);
char *strchrnul(const char *s, int c);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strpbrk(const char *s, const char *b);
char *strstr(const char *h, const char *n);

double strtod(const char *nptr, char **endptr);
float strtof(const char *nptr, char **endptr);
long double strtold(const char *nptr, char **endptr);


char *strerror(int errnum);


#ifdef __cplusplus
}
#endif

#endif
