#include <cpu.h>
#include <fs.h>
#include <fs/vfs.h>
#include <syscall.h>

int sys::chroot(const char *path) {
  auto proc = curproc;

  if (path == NULL || !proc->mm->validate_string(path)) return -EINVAL;

  scoped_lock l(proc->datalock);

  ck::ref<fs::Node> new_root = nullptr;

  if (0 != vfs::namei(path, 0, 0, proc->cwd, new_root)) return -1;

  if (new_root == nullptr) return -ENOENT;

  if (new_root->type != T_DIR) return -ENOTDIR;

  proc->root = new_root;


  ck::string cwd;
  if (vfs::getcwd(*new_root, cwd) != 0) return -EINVAL;
  proc->cwd_string = cwd;
  proc->cwd = new_root;

  return 0;
}
