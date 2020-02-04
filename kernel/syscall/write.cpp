#include <cpu.h>
#include <process.h>

ssize_t sys::write(int fd, void *data, long len) {
  auto proc = cpu::proc();

  int n = -1;

  if (!proc->mm.validate_pointer(data, len, VALIDATE_WRITE)) return -1;

  ref<fs::filedesc> file;

  proc->file_lock.lock();
  if (proc->open_files.contains(fd)) file = proc->open_files[fd].fd;
  proc->file_lock.unlock();

  if (file) n = file->write(data, len);
  return n;
}
