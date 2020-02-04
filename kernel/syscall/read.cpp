#include <cpu.h>
#include <process.h>
#include <util.h>

ssize_t sys::read(int fd, void *dst, long len) {
  int n = -1;
  ref<fs::filedesc> file = nullptr;
  auto proc = cpu::proc();

  if (proc->mm.validate_pointer(dst, len, VALIDATE_WRITE)) {
    scoped_lock lck(proc->file_lock);
    if (proc->open_files.contains(fd)) {
      file = proc->open_files[fd].fd;
    }
  }

  if (file) n = file->read(dst, len);

  return n;
}
