#include <cpu.h>
#include <process.h>

ssize_t sys::read(int fd, void *dst, size_t len) {
  auto proc = cpu::proc().get();

  if (!proc->mm.validate_pointer(dst, len, VALIDATE_WRITE)) {
    return -1;
  }

  return proc->read(fd, dst, len);
}
