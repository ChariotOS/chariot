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
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define BUFSIZ 1024
#define FILENAME_MAX 4096
#define FOPEN_MAX 1000
#define TMP_MAX 10000
#define L_tmpnam 20



#define __NEED_size_t
#define __NEED_ptrdiff_t
#include <bits/alltypes.h>
#include <stdarg.h>

int putchar(int);


int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char* buffer, size_t count, const char* format, ...);

int vsnprintf(char* buffer, size_t count, const char* format, va_list va);

#ifdef __cplusplus
}
#endif

#endif
