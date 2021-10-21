#include <errno.h>
#include <fs/vfs.h>
#include <cpu.h>

#include <process.h>
#include <thread.h>

int vfs::getcwd(fs::inode &cwd, ck::string &dst) {
  int err = 0;

  fs::inode *cur = &cwd;
  fs::inode *next = NULL;


  ck::string sep = "/";

  dst.clear();
  auto root = curproc->root;

  if (cur == root) {
    dst = "/";
  } else {
    int depth = 50;  // lol idk
    while (cur != root) {
      if (cur->type != T_DIR) return -ENOTDIR;

      next = cur->get_direntry("..");
      if (cur->dir.name == NULL) {
        return -EINVAL;
      }

      // this is terrible
      ck::string s = sep + cur->dir.name + dst;
      dst = s.get();
      cur = next;
      depth--;
      if (depth == 0) break;
    }
  }

  return err;
}
