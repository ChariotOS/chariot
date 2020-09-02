#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysbind.h>
#include <errno.h>

int fstat(int fd, struct stat *statbuf) {
  return errno_wrap(sysbind_fstat(fd, statbuf));
}

int stat(const char *path, struct stat *statbuf) {
  return errno_wrap(sysbind_lstat(path, statbuf));
}

int lstat(const char *path, struct stat *statbuf) {
  return errno_wrap(sysbind_lstat(path, statbuf));
}

int mkdir(const char *path, int mode) {
	errno = ENOTIMPL;
	return -1;
}
