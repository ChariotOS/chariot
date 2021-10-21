#include <cpu.h>
#include <time.h>
#include <syscall.h>
#include <util.h>

ssize_t sys::read(int fd, void *data, size_t len) {
  auto start = time::now_us();
  int n = -1;

  if (!curproc->mm->validate_pointer(data, len, PROT_WRITE)) return -EINVAL;

  ck::ref<fs::File> file = curproc->get_fd(fd);
  if (file) n = file->read(data, len);

  return n;
}
