#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "./impl.h"

static int _stdio_close(FILE *fp);
static size_t _stdio_read(FILE *fp, unsigned char *dst, size_t sz);
static size_t _stdio_write(FILE *fp, const unsigned char *src, size_t sz);
static off_t _stdio_seek(FILE *fp, off_t offset, int whence);

static FILE _stdin = {
    .close = NULL,
    .write = NULL,
    .seek = NULL,
    .read = _stdio_read,

    .fd = 0,
};

static char _stdout_buffer[512];

static FILE _stdout = {
    .close = NULL,
    .write = _stdio_write,
    .seek = NULL,
    .read = NULL,

    .fd = 1,

    .buffered = 1,
    .buf_len = 0,
    .buf_cap = 512,
    .buffer = _stdout_buffer,
};

static char _stderr_buffer[512];
static FILE _stderr = {
    .close = NULL,
    .write = _stdio_write,
    .seek = NULL,
    .read = NULL,

    .fd = 2,

    .buffered = 1,
    .buf_len = 0,
    .buf_cap = 512,
    .buffer = _stderr_buffer,
};

FILE *const stdin = &_stdin;
FILE *const stdout = &_stdout;
FILE *const stderr = &_stderr;

// TODO: lock
static FILE *ofl_head = NULL;

static FILE **ofl_lock() {
  // TODO: lock
  return &ofl_head;
}
static void ofl_unlock() {
  // TODO: unlock
}
static FILE *ofl_add(FILE *fp) {
  FILE **head = ofl_lock();
  fp->next = *head;
  if (*head) (*head)->prev = fp;
  *head = fp;
  ofl_unlock();
  return fp;
}

static void ofl_remove(FILE *fp) {
  FILE **head = ofl_lock();
  if (fp->prev) fp->prev->next = fp->next;
  if (fp->next) fp->next->prev = fp->prev;
  if (*head == fp) *head = fp->next;
  ofl_unlock();
}

static void stdio_exit(void) {
  fflush(stdout);
  fflush(stderr);

  for (FILE *f = ofl_head; f != NULL; f = f->next) {
    printf("flushing: %p\n", f);
    fflush(f);
  }
}

void stdio_init(void) { atexit(stdio_exit); }

static FILE *falloc();
static void ffree(FILE *);

FILE *falloc(void) {
  // allocate and zero out the file object
  FILE *fp = malloc(sizeof(*fp));
  memset(fp, 0, sizeof(*fp));

  fp->fd = -1;
  fp->lock = -1;

  return ofl_add(fp);
}

void ffree(FILE *fp) {
  // remove it from the open file list
  ofl_remove(fp);

  free(fp);
}

int fclose(FILE *fp) {
  if (fp->close != NULL) {
    fp->close(fp);
  }
  ffree(fp);
  return 0;
}

size_t fwrite(const void *restrict src, size_t size, size_t nmemb,
              FILE *restrict f) {
  size_t len = size * nmemb;

  FLOCK(f);

  if (f->write != NULL) {
    size_t k = f->write(f, src, len);

    if (k != 0) {
      FUNLOCK(f);
      return k / size;
    }
  }

  FUNLOCK(f);
  return 0;
}

size_t fread(void *restrict destv, size_t size, size_t nmemb,
             FILE *restrict f) {
  size_t len = size * nmemb;

  FLOCK(f);

  if (f->read != NULL) {
    size_t k = f->read(f, destv, len);

    if (k < 0) {
      f->eof = 1;
      FUNLOCK(f);
      return 0;
    }

    if (k != 0) {
      FUNLOCK(f);
      return k / size;
    }
  }

  FUNLOCK(f);
  return 0;
}

static int _stdio_close(FILE *fp) {
  // close both ends of the file?
  errno_syscall(SYS_close, fp->fd);

  return 0;
}

static size_t _stdio_read(FILE *fp, unsigned char *dst, size_t sz) {
  if (fp->fd == -1) {
    return 0;
  }
  long k = errno_syscall(SYS_read, fp->fd, dst, sz);
  if (k < 0) return 0;
  return k;
}

static void _stdio_flush_buffer(FILE *fp) {
  if (fp->buffered && fp->buffer != NULL && fp->buf_len > 0) {
    errno_syscall(SYS_write, fp->fd, fp->buffer, fp->buf_len);
    fp->buf_len = 0;
    // NULL out the buffer
    memset(fp->buffer, 0x00, fp->buf_cap);
  }
}

static size_t _stdio_write(FILE *fp, const unsigned char *src, size_t sz) {
  if (fp->fd == -1) {
    return 0;
  }

  if (fp->buffered && fp->buffer != NULL) {
    /**
     * copy from the src into the buffer, flushing on \n or when it is full
     */
    for (size_t i = 0; i < sz; i++) {
      // Assume that the buffer has space avail.
      unsigned char c = src[i];

      fp->buffer[fp->buf_len++] = c;

      if (c == '\n' || /* is full */ fp->buf_len == fp->buf_cap) {
        _stdio_flush_buffer(fp);
      }
    }
    return sz;
  }

  long k = errno_syscall(SYS_write, fp->fd, src, sz);
  if (k < 0) return 0;
  return k;
}

static off_t _stdio_seek(FILE *fp, off_t offset, int whence) {
  return errno_syscall(SYS_lseek, fp->fd, offset, whence);
}

FILE *fdopen(int fd, const char *mode) {
  FILE *fp = falloc();

  char im = *mode;

  if (im != 'r' && im != 'w' && im != 'a') {
    // TODO: errno = EINVAL;
    return NULL;
  }

  // int readable = 1;
  // int writable = 1;

  // TODO: more modes!

  fp->fd = fd;

  fp->close = _stdio_close;
  fp->read = _stdio_read;
  fp->write = _stdio_write;
  fp->seek = _stdio_seek;

  fp->buffered = 0;

  return fp;
}

FILE *fopen(const char *path, const char *mode) {
  // TODO: better mode here
  int fd = open(path, O_RDWR);

  return fdopen(fd, mode);
}

int fflush(FILE *fp) {
  _stdio_flush_buffer(fp);
  return 0;
}

int fputc(int c, FILE *stream) {
  char data[] = {c};
  fwrite(data, 1, 1, stream);
  return c;
}

int putc(int c, FILE *stream) __attribute__((weak, alias("fputc")));

int fgetc(FILE *stream) {
  char buf[1];
  int r;
  r = fread(buf, 1, 1, stream);
  if (r < 0) {
    stream->eof = 1;
    return EOF;
  } else if (r == 0) {
    stream->eof = 1;
    return EOF;
  }
  return (unsigned char)buf[0];
}

int feof(FILE *stream) { return stream->eof; }

int getc(FILE *stream) __attribute__((weak, alias("fgetc")));

int getchar(void) { return fgetc(stdin); }

char *fgets(char *s, int size, FILE *stream) {
  int c;
  char *out = s;
  while ((c = fgetc(stream)) > 0) {
    *s++ = c;
    size--;
    if (size == 0) {
      return out;
    }
    *s = '\0';
    if (c == '\n') {
      return out;
    }
  }
  if (c == EOF) {
    stream->eof = 1;
    if (out == s) {
      return NULL;
    } else {
      return out;
    }
  }
  return NULL;
}

int putchar(int c) { return fputc(c, stdout); }

int fseek(FILE *stream, long offset, int whence) {
  return lseek(stream->fd, offset, whence);
}

long ftell(FILE *stream) { return lseek(stream->fd, 0, SEEK_CUR); }

void rewind(FILE *stream) { fseek(stream, 0, SEEK_SET); }

int fputs(const char *s, FILE *stream) {
  int len = strlen(s);
  int r = fwrite(s, len, 1, stream);
  return r >= 0 ? 0 : EOF;
}

int puts(const char *s) { return fputs(s, stdout); }

int remove(const char *pathname) { return -1; }

void perror(const char *msg) {
  fwrite(msg, strlen(msg), 1, stderr);
  fputc(':', stderr);
  fputc(' ', stderr);

  switch (errno) {
#define E(a, b)       \
  case a:             \
    fputs(b, stderr); \
    break;
#include "__strerror.h"

    default:
      fputs("No error information", stderr);
  }
  fputc('\n', stderr);
}
