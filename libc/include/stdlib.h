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


int atoi(const char *);
long atol(const char *);
long long atoll(const char *);
double atof(const char *);



void free(void *);

void *malloc(size_t sz) __attribute__((malloc));
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);


unsigned long long malloc_total(void);
unsigned long long malloc_allocated(void);


int system(const char *);

void exit(int status);
int atexit(void (*function)(void));


char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);


void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

void abort(void);

char *ptsname(int fd);

#define RAND_MAX (0x7fffffff)
int rand(void);
void srand(unsigned);

// nonstandard chariot functions
#ifdef _CHARIOT_SRC
// returns the joined path of a and b. Must be freed
char *path_join(char *a, char *b);

// pretty print hexdump to the terminal
void debug_hexdump(void *, size_t);
void debug_hexdump_grouped(void *, size_t, int grouping);
#endif



#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0


long int strtol(const char *nptr, char **endptr, int base);
long long int strtoll(const char *nptr, char **endptr, int base);


int posix_memalign(void **memptr, size_t alignment, size_t size);


void *bsearch(const void *key, const void *base, size_t nel, size_t width, int (*cmp)(const void *, const void *));


#ifdef __cplusplus
}
#endif

#endif
