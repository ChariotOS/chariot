#ifndef _STDIO_IMPL_H
#define _STDIO_IMPL_H

#include <stdio.h>
#include <stdint.h>
// #include "syscall.h"

#define UNGET 8

#define FFINALLOCK(f) ((f)->lock >= 0 ? __lockfile((f)) : 0)

#define FLOCK(f)
#define FUNLOCK(f)

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


  int buf_len, buf_cap;
	char default_buffer[BUFSIZ];
  char *buffer;
};



#endif
