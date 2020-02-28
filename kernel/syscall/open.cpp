#include <process.h>
#include <cpu.h>

int sys::open(const char *path, int flags, int mode) {
  auto proc = cpu::proc();

  if (!proc->mm->validate_string(path)) {
    return -1;
  }


  auto ino = vfs::open(path, flags, mode);
  if (ino == NULL) return -ENOENT;

  auto file = fs::file::create(ino, path, flags);

  int fd = proc->add_fd(file);

  return fd;
}
