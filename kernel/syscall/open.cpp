#include <syscall.h>
#include <cpu.h>

int sys::open(const char *path, int flags, int mode) {
  auto proc = cpu::proc();

  if (!proc->mm->validate_string(path)) {
    return -1;
  }

  struct fs::inode *ino = NULL;


  int r = vfs::namei(path, flags, mode, vfs::cwd(), ino);
  if (ino == NULL) return -ENOENT;

  if (r < 0) {
    return r;
  }

  auto file = fs::file::create(ino, path, flags);
  int fd = proc->add_fd(file);

  return fd;
}
