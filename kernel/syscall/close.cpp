#include <process.h>
#include <cpu.h>

int sys::close(int fd) {
  auto proc = cpu::proc();
  assert(proc != NULL);

  int err = -ENOENT;
  proc->file_lock.lock();
  if (proc->open_files.contains(fd)) {
    proc->open_files.remove(fd);
    err = 0;
  }
  proc->file_lock.unlock();

  return err;
}
