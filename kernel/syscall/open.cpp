#include <process.h>
#include <cpu.h>

int sys::open(const char *path, int flags, int mode) {
  auto proc = cpu::proc();

  if (!proc->mm.validate_string(path)) {
    return -1;
  }

  return proc->open(path, flags, mode);
}
