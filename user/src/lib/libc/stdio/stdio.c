#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#include "./impl.h"


FILE *const stdin;
FILE *const stdout;
FILE *const stderr;

void stdio_init(void) {
  *(FILE **)&stdin = fdopen(0, "r");
  stdin->close = NULL;
  stdin->write = NULL;
  stdin->seek = NULL;


  *(FILE **)&stdout = fdopen(1, "w");
  stdout->read = NULL;
  stdout->seek = NULL;


  *(FILE **)&stderr = fdopen(2, "w");
  stderr->read = NULL;
  stderr->seek = NULL;

}

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

static FILE *falloc();
static void ffree(FILE *);

FILE *falloc(void) {
  // allocate and zero out the file object
  FILE *fp = malloc(sizeof(*fp));
  memset(fp, 0, sizeof(*fp));

  fp->wfd = -1;
  fp->rfd = -1;
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

  printf("len=%d\n", len);

  FLOCK(f);

  if (f->read != NULL) {
    size_t k = f->read(f, destv, len);

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
  if (fp->wfd != fp->rfd) {
    syscall(SYS_close, fp->wfd);
    syscall(SYS_close, fp->wfd);
  } else {
    syscall(SYS_close, fp->wfd);
  }

  return 0;
}

static size_t _stdio_read(FILE *fp, unsigned char *dst, size_t sz) {
  if (fp->rfd == -1) {
    return 0;
  }
  long k = syscall(SYS_read, fp->rfd, dst, sz);
  if (k < 0) return 0;
  return k;
}

static size_t _stdio_write(FILE *fp, const unsigned char *src, size_t sz) {
  if (fp->rfd == -1) {
    return 0;
  }
  long k = syscall(SYS_write, fp->rfd, src, sz);
  if (k < 0) return 0;
  return k;
}

static off_t _stdio_seek(FILE *fp, off_t offset, int whence) {
  return syscall(SYS_lseek, fp->rfd, offset, whence);
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

  fp->rfd = fd;
  fp->wfd = fd;

  fp->close = _stdio_close;
  fp->read = _stdio_read;
  fp->write = _stdio_write;
  fp->seek = _stdio_seek;

  return fp;
}
