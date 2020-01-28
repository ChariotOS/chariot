#include <process.h>
#include <cpu.h>

int sys::close(int fd) {
  auto proc = cpu::proc().get();
  return proc->close(fd);
}
