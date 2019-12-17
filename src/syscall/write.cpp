#include <process.h>
#include <cpu.h>

ssize_t sys::write(int fd, void *data, size_t len) {

  auto proc = cpu::proc().get();

  if (!proc->mm.validate_pointer(data, len, VALIDATE_WRITE)) {
    return -1;
  }

  return proc->write(fd, data, len);
}
