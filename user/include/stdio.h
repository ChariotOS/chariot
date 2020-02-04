#pragma once

#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#undef NULL

#ifdef __cplusplus
#define NULL 0L
#else
#define NULL ((void*)0)
#endif

#undef EOF
#define EOF (-1)

#undef SEEK_SET
#undef SEEK_CUR
#undef SEEK_END
#define SEEK_SET (-1)
#define SEEK_CUR (-2)
#define SEEK_END (-3)


#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define BUFSIZ 1024
#define FILENAME_MAX 4096
#define FOPEN_MAX 1000
#define TMP_MAX 10000
#define L_tmpnam 20


#define __NEED_FILE
#define __NEED___isoc_va_list
#define __NEED_size_t
#define __NEED_ptrdiff_t
#define __NEED_off_t

#include <bits/alltypes.h>


#include <stdarg.h>

extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;




FILE *fdopen(int, const char *);
FILE *fopen(const char *pathname, const char *mode);
int fclose(FILE *);


size_t fread(void *__restrict, size_t, size_t, FILE *__restrict);
size_t fwrite(const void *__restrict, size_t, size_t, FILE *__restrict);

int fflush(FILE *);

int putchar(int);



int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char* buffer, size_t count, const char* format, ...);

int vsnprintf(char* buffer, size_t count, const char* format, va_list va);

#ifdef __cplusplus
}
#endif

#endif
