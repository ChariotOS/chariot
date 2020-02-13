#include <cpu.h>
#include <process.h>
#include <stat.h>

int sys::stat(const char *pathname, struct stat *statbuf) { return -1; }

int sys::fstat(int fd, struct stat *statbuf) {
  auto proc = cpu::proc();

  if (!proc->addr_space->validate_pointer(statbuf, sizeof(*statbuf), VPROT_READ | VPROT_WRITE)) return -1;

  auto file = proc->get_fd(fd);

  if (!file) return -ENOENT;

  // defer to the file's inode
  if (file->ino) return file->ino->stat(statbuf);
  return -1;
}

int sys::lstat(const char *pathname, struct stat *statbuf) {
  if (!curproc->addr_space->validate_string(pathname)) return -1;
  if (!curproc->addr_space->validate_pointer(statbuf, sizeof(*statbuf), VPROT_READ)) return -1;


  struct fs::inode *ino;

  // TODO: flags!
  if (vfs::namei(pathname, 0, 0, curproc->cwd, ino) != 0) return -1;

  if (ino) return ino->stat(statbuf);
  return -1;
}
