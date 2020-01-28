#include <cpu.h>
#include <process.h>

ssize_t sys::write(int fd, void *data, size_t len) {
  auto proc = cpu::proc().get();

  int n = -1;

  if (!proc->mm.validate_pointer(data, len, VALIDATE_WRITE)) return -1;

  proc->file_lock.lock();
  auto &of = proc->open_files;
  if (of.contains(fd)) n = of.get(fd).fd->write(data, len);
  proc->file_lock.unlock();
  return n;
}
