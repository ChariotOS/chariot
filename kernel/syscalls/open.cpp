#include <syscall.h>
#include <cpu.h>

int sys::open(const char *path, int flags, int mode) {
  auto proc = cpu::proc();

  if (!proc->mm->validate_string(path)) {
    return -1;
  }

  struct fs::Node *ino = NULL;


  int r = vfs::namei(path, flags, mode, vfs::cwd(), ino);

  if (ino == NULL) return -ENOENT;

  if (r < 0) {
    return r;
  }

  auto file = fs::File::create(ino, path, flags);
  if (file->ino == NULL) {
    // the file was created for no reason, as it failed to open
    return file->errorcode();  // negative errno
  }
  int fd = proc->add_fd(file);

  return fd;
}
