#ifndef _STDIO_IMPL_H
#define _STDIO_IMPL_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
// #include "syscall.h"

#define UNGET 8

#define FFINALLOCK(f) ((f)->lock >= 0 ? __lockfile((f)) : 0)

#define FLOCK(f) pthread_mutex_lock(&f->flock)
#define FUNLOCK(f) pthread_mutex_unlock(&f->flock)

#define F_PERM 1
#define F_NORD 4
#define F_NOWR 8
#define F_EOF 16
#define F_ERR 32
#define F_SVB 64
#define F_APP 128

#define FILE_BUFSZ 512


#define BUFROLE_NONE 0
#define BUFROLE_WRITE 1
#define BUFROLE_READ 2


size_t _fwrite(const void *restrict src, size_t size, size_t nmemb, FILE *restrict f);
size_t _fread(void *restrict destv, size_t size, size_t nmemb, FILE *restrict f);


struct _FILE_IMPL {
  // possible different file descriptors for reading and writing
  int fd;

  // inline linked list
  FILE *next, *prev;

  int flags;
  int eof;
  // the offset into the file
  int pos;

  // these operations can be ``overloaded'' in a sense. If the pointer is NULL,
  // it is not allowed. For example, a readonly file will have the read function
  // but no write. Likewise, non-closable functions will have `.close=NULL`
  int (*close)(FILE *);
  size_t (*read)(FILE *, unsigned char *, size_t);
  size_t (*write)(FILE *, const unsigned char *, size_t);
  off_t (*seek)(FILE *, off_t, int);

  int lock;

  int buffered;
  int bufrole;
  // if bufrole == BUFROLE_READ, this describes the byte offset of
  // the buffer into the file.
  int bufbase;


	int has_ungetc;
	char ungetc;


  int buf_len, buf_cap;
  char default_buffer[BUFSIZ];
  char *buffer;
  pthread_mutex_t flock;
};



#endif
