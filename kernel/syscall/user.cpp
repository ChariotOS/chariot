#include <syscall.h>
#include <cpu.h>

int sys::getuid(void) {
  return curproc->user.uid;
}
int sys::geteuid(void) {
  return curproc->user.euid;
}

int sys::getgid(void) {
  return curproc->user.gid;
}
int sys::getegid(void) {
  return curproc->user.egid;
}
