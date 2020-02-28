#include <syscall.h>
#include <cpu.h>

long sys::getuid(void) {
  return curproc->user.uid;
}
long sys::geteuid(void) {
  return curproc->user.euid;
}

long sys::getgid(void) {
  return curproc->user.gid;
}
long sys::getegid(void) {
  return curproc->user.egid;
}
