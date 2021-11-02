#include <cpu.h>
#include <fs.h>
#include <fs/vfs.h>
#include <syscall.h>

int sys::chdir(const char *path) {
  auto proc = curproc;

  if (path == NULL || !proc->mm->validate_string(path)) return -EINVAL;

  scoped_lock l(proc->datalock);

  ck::ref<fs::Node> ncwd = nullptr;

  if (0 != vfs::namei(path, 0, 0, proc->cwd, ncwd)) return -1;

  if (ncwd == nullptr) return -ENOENT;
  if (ncwd->type != T_DIR) return -ENOTDIR;


  ck::string cwd;
  if (vfs::getcwd(*ncwd, cwd) != 0) return -EINVAL;

  proc->cwd_string = cwd;
  proc->cwd = ncwd;

  return 0;
}

int sys::getcwd(char *dst, int dlen) {
  if (!curproc->mm->validate_pointer(dst, dlen, PROT_WRITE)) return -1;

  ck::string s;
  int res = vfs::getcwd(*curproc->cwd, s);

  if (res == 0) {
    int l = s.size() + 1;
    if (dlen < l) l = dlen;
    memcpy(dst, s.get(), l);
    dst[l] = 0;
  }

  return res;
}
