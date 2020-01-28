#include <process.h>
#include <task.h>
#include <cpu.h>

int sys::dup(int fd) {

  auto proc = cpu::proc().get();

  return proc->do_dup(fd, -1);
}

int sys::dup2(int oldfd, int newfd) {
  if (oldfd < 0) return -1;
  if (newfd < 0) return -1;

  auto proc = cpu::proc().get();
  return proc->do_dup(oldfd, newfd);
}
