#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

ssize_t write(int fd, const void *buf, size_t count) {
  // just forward to the system call
  return syscall(SYS_write, fd, buf, count);
}


ssize_t read(int fd, void *buf, size_t count) {
  // just forward to the system call
  return syscall(SYS_read, fd, buf, count);
}


off_t lseek(int fd, off_t offset, int whence) {
  return syscall(SYS_lseek, fd, offset, whence);
}


int close(int fd) {
  return syscall(SYS_close, fd);
}


int chdir(const char *path) {
  int res = syscall(SYS_chdir, path);

  // printf("chdir('%s') = %d\n", path, res);

  return res;
}

