#include <process.h>
#include <cpu.h>

off_t sys::lseek(int fd, off_t offset, int whence) {
  auto proc = cpu::proc().get();

  auto f = proc->get_fd(fd);

  if (fd) {
    return f->seek(offset, whence);
  }
  return -ENOENT;
}
