#include <cpu.h>
#include <syscall.h>

ssize_t sys::write(int fd, void *data, size_t len) {
  int n = -1;

  if (!curproc->mm->validate_pointer(data, len, PROT_READ))
    return -1;

  ref<fs::file> file = curproc->get_fd(fd);

  if (file) n = file->write(data, len);
  return n;
}
