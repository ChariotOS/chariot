#include <cpu.h>
#include <fs.h>
#include <fs/vfs.h>
#include <syscall.h>

int sys::chroot(const char *path) {
  auto proc = curproc;

  if (path == NULL || !proc->mm->validate_string(path)) return -EINVAL;

	return vfs::chroot(path);
}
