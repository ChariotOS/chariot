#include <process.h>
#include <stat.h>


int sys::stat(const char *pathname, struct stat *statbuf) {
  return -1;
}


int sys::fstat(int fd, struct stat *statbuf) {
  return -1;
}


int sys::lstat(const char *pathname, struct stat *statbuf) {
  return -1;
}
