#include <sys/stat.h>
#include <sys/syscall.h>

int fstat(int fd, struct stat *statbuf) {
  return syscall(SYS_fstat, fd, statbuf);
}
