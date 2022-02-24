#include <errno.h>
#include <fs/vfs.h>
#include <cpu.h>

#include <process.h>
#include <thread.h>

int vfs::getcwd(fs::Node &cwd, ck::string &dst) {
  int err = 0;

  ck::ref<fs::Node> cur = &cwd;
  ck::ref<fs::Node> next = nullptr;


  ck::string sep = "/";

  dst.clear();
  auto root = curproc->root;

  if (cur == root) {
    dst = "/";
  } else {
    int depth = 500;  // lol idk
    while (cur != root) {
      if (!cur->is_dir()) return -ENOTDIR;

      next = cur->get_direntry("..")->get();
      if (cur->name() == "") {
				KWARN("getcwd: node has empty name\n");
        return -EINVAL;
      }

      // this is terrible
      ck::string s = sep + cur->name() + dst;
      dst = s.get();
      cur = next;
      depth--;
      if (depth == 0) break;
    }
  }

  return err;
}
