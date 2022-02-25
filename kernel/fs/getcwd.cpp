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
  ck::string path;
  auto root = curproc->root;

  ck::vec<ck::string> parts_rev;

  while (cur != root) {
    if (!cur->is_dir()) {
      return -ENOTDIR;
    }

    next = cur->get_direntry("..")->get();

    auto dents = next->dirents();
    ck::string curname;
    for (auto dent : dents) {
      if (dent->get() == cur) {
        curname = dent->name;
      }
    }

    parts_rev.push(curname);
    if (next == cur) break;
    cur = next;
  }


  if (parts_rev.size() == 0) {
    path = "/";
  } else {
    for (int i = parts_rev.size() - 1; i >= 0; i--) {
      path += "/";
      path += parts_rev[i];
    }
  }

  dst = path;
  return err;
}
