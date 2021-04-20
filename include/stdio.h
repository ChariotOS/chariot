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
#define NULL ((void *)0)
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

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define __NEED_FILE
#define __NEED___isoc_va_list
#define __NEED_size_t
#define __NEED_ssize_t
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


int fileno(FILE *stream);

size_t fread(void *__restrict, size_t, size_t, FILE *__restrict);
size_t fwrite(const void *__restrict, size_t, size_t, FILE *__restrict);

int fgetc(FILE *stream);

int fflush(FILE *);

int putchar(int);

int getchar(void);
char *fgets(char *s, int size, FILE *stream);
int getchar(void);



int ungetc(int c, FILE *stream);


int fputc(int c, FILE *stream);
int putc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int puts(const char *s);

int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *buffer, size_t count, const char *format, ...);
int vsnprintf(char *buffer, size_t count, const char *format, va_list va);
int vsprintf(char *buffer, const char *format, va_list va);

int fprintf(FILE *fp, const char *fmt, ...);
int vfprintf(FILE *fp, const char *fmt, va_list ap);
int vsnfprintf(FILE *fp, const char *fmt, va_list va);

int sscanf(const char *, const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);

int feof(FILE *stream);
int ferror(FILE *);
void clearerr(FILE *);

int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);

int remove(const char *pathname);



void setbuf(FILE *stream, char *buf);
void setbuffer(FILE *stream, char *buf, size_t size);
void setlinebuf(FILE *stream);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);

void perror(const char *s);


int rename(const char *old_filename, const char *new_filename);



ssize_t getdelim(char **lineptr, size_t *n, int delimiter, FILE *stream);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);


// STUB
FILE *tmpfile(void);



#ifdef __cplusplus
}
#endif

#endif
