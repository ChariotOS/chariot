#include <syscall.h>
#include <cpu.h>
#include <fs/magicfd.h>

int sys::close(int fd) {
  if ((fd & MAGICFD_MASK) != 0) return -EPERM;

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
