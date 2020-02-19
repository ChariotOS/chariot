#include <process.h>
#include <fs.h>
#include <cpu.h>

int sys::chdir(const char *path) {
  auto proc = curproc;

  if (!proc->addr_space->validate_string(path)) return -EINVAL;

  scoped_lock l(proc->datalock);

  fs::inode *ncwd = NULL;

  if (0 != vfs::namei(path, 0, 0, proc->cwd, ncwd)) return -1;

  if (ncwd == NULL)
    return -ENOENT;

  if (ncwd->type != T_DIR) return -ENOTDIR;

  fs::inode::acquire(ncwd);
  fs::inode::release(proc->cwd);

  proc->cwd = ncwd;


  return 0;
}
