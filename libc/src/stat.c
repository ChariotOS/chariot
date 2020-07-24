#include <sys/stat.h>
#include <sys/syscall.h>
#include <errno.h>

int fstat(int fd, struct stat *statbuf) {
  return errno_syscall(SYS_fstat, fd, statbuf);
}

int lstat(const char *path, struct stat *statbuf) {
  return errno_syscall(SYS_lstat, path, statbuf);
}

int mkdir(const char *path, int mode) {
	errno = ENOTIMPL;
	return -1;
}
