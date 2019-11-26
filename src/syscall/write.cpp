#include <process.h>
#include <cpu.h>

ssize_t sys::write(int fd, void *data, size_t len) {
  auto proc = cpu::proc();

  if (!proc->mm.validate_pointer(data, len, VALIDATE_READ)) {
    return -1;
  }

  for (int i = 0; i < len; i++) {
    printk("%c", ((char *)data)[i]);
  }
  return 0;
}
