#include <cpu.h>
#include <process.h>
#include <util.h>

ssize_t sys::read(int fd, void *data, long len) {
  int n = -1;

  if (!curproc->addr_space->validate_pointer(data, len, VALIDATE_WRITE)) return -1;

  ref<fs::filedesc> file = curproc->get_fd(fd);

  if (file) n = file->read(data, len);
  return n;
}
