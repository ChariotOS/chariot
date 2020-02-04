#include <process.h>
#include <cpu.h>

long sys::lseek(int fd, long offset, int whence) {
  auto proc = cpu::proc();

  auto f = proc->get_fd(fd);

  if (fd) {
    return f->seek(offset, whence);
  }
  return -ENOENT;
}
