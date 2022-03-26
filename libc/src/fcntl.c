#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/sysbind.h>
#include <stdarg.h>
#include <stdio.h>


// TODO
int creat(const char *pth, mode_t mod) { return -1; }


// TODO
int fcntl(int fd, int cmd, ...) { return -1; }

int openat(int dirfd, const char *name, int flags, ...) {
	printf("openat\n");
	return -ENOTIMPL;
}

int open(const char *filename, int flags, ...) {
  mode_t mode = 0;

  if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  int fd = sysbind_open(filename, flags, mode);
  // if (fd>=0 && (flags & O_CLOEXEC)) {
  //  __syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);
  // }


  return __syscall_ret(fd);
}
