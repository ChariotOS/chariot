#include <cpu.h>
#include <syscall.h>

int sys::unlink(const char *path) {
  if (!curproc->mm->validate_string(path)) {
    return -EINVAL;
  }

  // just defer to the vfs
  return vfs::unlink(path, curproc->cwd);
}
