#include <cpu.h>
#include <syscall.h>
#include <util.h>

ssize_t sys::read(int fd, void *data, long len) {
  int n = -1;

  if (!curproc->mm->validate_pointer(data, len, PROT_WRITE)) return -1;

  ref<fs::file> file = fget(fd);
  if (file) {
    n = file->read(data, len);
  }
  return n;
}
