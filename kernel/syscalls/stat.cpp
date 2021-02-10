#include <cpu.h>
#include <stat.h>
#include <syscall.h>

int sys::stat(const char *pathname, struct stat *statbuf) {
  return -1;
}

int sys::fstat(int fd, struct stat *statbuf) {
  auto proc = cpu::proc();

  if (!proc->mm->validate_pointer(statbuf, sizeof(*statbuf), PROT_READ | PROT_WRITE)) return -1;

  auto file = proc->get_fd(fd);

  if (!file) return -ENOENT;

  // defer to the file's inode
  if (file->ino) return file->ino->stat(statbuf);
  return -1;
}

int sys::lstat(const char *pathname, struct stat *statbuf) {
  if (!curproc->mm->validate_string(pathname)) return -1;
  if (!curproc->mm->validate_pointer(statbuf, sizeof(*statbuf), PROT_READ)) return -1;


  struct fs::inode *ino;

  int err = vfs::namei(pathname, 0, 0, curproc->cwd, ino);
  // TODO: flags!
  if (err != 0) return err;

  if (ino) return ino->stat(statbuf);
  return -1;
}
