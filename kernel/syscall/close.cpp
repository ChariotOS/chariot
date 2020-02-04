#include <process.h>
#include <cpu.h>

int sys::close(int fd) {
  auto proc = cpu::proc();
  assert(proc != NULL);

  return proc->close(fd);
}
