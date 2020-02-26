#include <cpu.h>
#include <process.h>

ssize_t sys::write(int fd, void *data, long len) {
  int n = -1;

  if (!curproc->mm->validate_pointer(data, len, PROT_READ))
    return -1;

  ref<fs::filedesc> file = curproc->get_fd(fd);

  if (file) n = file->write(data, len);
  return n;
}
