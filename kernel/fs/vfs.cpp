#include <cpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <ck/map.h>
#include <syscall.h>
#include <module.h>
#include <util.h>

#include <thread.h>


#define LOG(...) PFXLOG(GRN "VFS", __VA_ARGS__)

ck::ref<fs::Node> vfs_root = nullptr;
static ck::vec<struct fs::SuperBlockInfo *> filesystems;
static ck::vec<struct vfs::mountpoint *> mountpoints;

void vfs::register_filesystem(struct fs::SuperBlockInfo &info) {
  LOG("filesystem '%s' registered\n", info.name);
  filesystems.push(&info);
}

void vfs::deregister_filesystem(struct fs::SuperBlockInfo &) {}

ck::ref<fs::Node> vfs::cwd(void) {
  if (cpu::in_thread()) {
    auto proc = cpu::proc();

    if (proc->cwd) {
      return proc->cwd;
    }
  }

  return vfs_root;
}

ck::ref<fs::Node> vfs::get_root(void) { return vfs_root; }

int vfs::mount_root(const char *src, const char *type) {
  panic("mounting root %s with type %s\n", src, type);
  return vfs::mount(src, "/", type, 0, 0);
}

int vfs::mount(const char *src, const char *targ, const char *type, unsigned long flags, const char *options) {
  // printk(KERN_INFO "mount %s with fs %s to %s\n", src, type, targ);

  if (get_root().is_null() && strcmp(targ, "/") != 0) {
    LOG("[WARN] Mounting non-root filesystem when there is no root is invalid");
    return -EINVAL;
  }

  if (type == NULL) return -EINVAL;

  if (get_root()) {
    // TODO: look up the target directory
  }

  struct fs::SuperBlockInfo *fs = nullptr;

  for (int i = 0; i < filesystems.size(); i++) {
    if (strcmp(type, filesystems[i]->name) == 0) {
      fs = filesystems[i];
      break;
    }
  }

  if (fs == nullptr) {
    LOG("failed to find the filesystem for that name\n");
    return -ENOENT;
  }
  auto sb = fs->mount(fs, options, flags, src);
  if (sb == nullptr) {
    LOG("failed to mount filesystem\n");
    return -EINVAL;
  }

  auto mp = new vfs::mountpoint();
  mp->sb = sb;
  mp->mountflags = flags;
  mp->devname = targ;
  mp->id = 0;

  assert(sb->root);

  assert(mp->sb->root->sb == sb);

  if (get_root() == nullptr && strcmp(targ, "/") == 0) {
    // update the root
    // TODO: this needs to be smarter :)
    vfs_root = mp->sb->root;
    sched::proc::kproc()->cwd = vfs_root;
    sched::proc::kproc()->root = vfs_root;
  } else {
    // this is so gross...
    mp->host = vfs::open(targ, O_RDONLY, 0);
    if (mp->host == nullptr || !mp->host->is_dir() || mp->host == vfs_root) {
      delete mp;
      return -EINVAL;
    }

    auto parent_dirent = mp->host->get_direntry("..");
    assert(parent_dirent != nullptr);
    if (parent_dirent->mount_shadow != nullptr) {
      delete mp;
      return -EEXIST;
    }

    auto parent = parent_dirent->ino;
    assert(parent != nullptr);

    {
      auto l = mp->sb->root->irq_lock();
      auto ent = mp->sb->root->get_direntry("..");
      ent->mount_shadow = parent;
    }

    // copy the last entry into the name of the guest root
    auto end = ck::string(targ).split('/').last();
    auto name = calloc<char>(end.size() + 1);
    memcpy(name, end.get(), end.size() + 1);
    mp->sb->root->set_name(name);

    // and mount it with a shadow node
    auto mountpoint_dirent = parent->get_direntry(end.get());
    assert(mountpoint_dirent->mount_shadow == nullptr);
    mountpoint_dirent->mount_shadow = mp->sb->root;
  }

  mountpoints.push(mp);
  return 0;
}

int sys::mount(struct mountopts *opts) {
  if (!curproc->mm->validate_pointer(opts, sizeof(*opts), PROT_WRITE)) return -EINVAL;
  /* TODO: permissions :) */
  if (curproc->user.uid != 0) return -EPERM;

  /* TODO: take the last argument for real! */
  return vfs::mount(opts->device, opts->dir, opts->type, opts->flags, "");
}




int sys::mkdir(const char *upath, int mode) {
  if (!curproc->mm->validate_string(upath)) return -EINVAL;

  char path[256];
  strncpy(path, upath, sizeof(path));

  int last_slash = -1;
  for (int i = 0; path[i] != '\0'; i++)
    if (path[i] == '/') last_slash = i;

  // by default, the name is the whole path (no slash in the provided slash)
  const char *name = path;
  // if there was a slash in the name, fix up the name and the path so it points after
  if (last_slash != -1) name = path + last_slash + 1;

  ck::ref<fs::Node> dir = nullptr;
  // get the last directory in the name, (hence the true)
  int err = vfs::namei(path, 0, mode, vfs::cwd(), dir, true);
  if (err < 0) return err;
  if (dir.is_null()) return -ENOENT;
  if (!dir->is_dir()) return -ENOTDIR;

  // check that the file doesn't exist first. This means we don't
  // have to check in the filesystem driver.
  if (dir->get_direntry(name) != nullptr) return -EEXIST;


  fs::Ownership fown;
  // TODO: real permissions
  fown.uid = curproc->user.uid;
  fown.gid = curproc->user.gid;
  fown.mode = mode;  // from the argument passed from the user
  int res = dir->mkdir(name, fown);

  return res;
}




ck::ref<fs::Node> vfs::open(ck::string spath, int opts, int mode) {
  ck::ref<fs::Node> ino = nullptr;

  if (!cpu::in_thread()) {
    LOG("not in thread\n");
  }

  if (0 != vfs::namei(spath.get(), opts, mode, vfs::cwd(), ino)) {
    return nullptr;
  }
  return ino;
}

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static const char *skipelem(const char *path, char *name, bool &last) {
  const char *s;
  int len;

  while (*path == '/')
    path++;
  if (*path == 0) return 0;
  s = path;
  last = false;
  while (1) {
    if (*path == '/') break;
    if (*path == 0x0) {
      last = true;
      break;
    }
    path++;
  }
  len = path - s;
  if (name != NULL) {
    if (len >= 255)
      memcpy(name, s, 255);
    else {
      memcpy(name, s, len);
      name[len] = 0;
    }
  }
  while (*path == '/')
    path++;
  return path;
}

int vfs::namei(const char *path, int flags, int mode, ck::ref<fs::Node> cwd, ck::ref<fs::Node> &res, bool get_last) {
  assert(path != NULL);
  auto ino = cwd;

  auto uroot = curproc->root ?: vfs::get_root();
  /* if the path is rooted (/ at start), set the "working cwd" to the root
   * directory */
  if (path[0] == '/') {
    ino = uroot;
  } else {
    assert(cwd != nullptr);
  }

  char name[256];
  bool last = false;


  int i = 0;
  while ((path = skipelem((char *)path, name, last)) != 0) {
    // attempting to go above the current root is a nop
    if (!strcmp(name, "..") && ino == uroot) {
      continue;
    }

    if (last && get_last) {
      res = ino;
      return 0;
    }

    auto found = ino->get_direntry(name);

    if (found == nullptr) {
      if (last && (flags & O_CREAT)) {
        fs::Ownership own;
        own.uid = curproc->user.uid;
        own.gid = curproc->user.gid;
        own.mode = mode;
        int r = ino->touch(name, own);
        if (r == 0) {
          res = ino->get_direntry(name)->ino;
        }
        return r;
      }

      res = nullptr;
      return -ENOENT;
    }

    ino = found->ino;
  }

  res = ino;
  return 0;
}



int vfs::unlink(const char *path, ck::ref<fs::Node> cwd) {
  auto ino = cwd;

  auto uroot = curproc->root ?: vfs::get_root();
  if (path[0] == '/') {
    ino = uroot;
  } else {
    assert(cwd != nullptr);
  }

  char name[256];
  bool last = false;

  while ((path = skipelem((char *)path, name, last)) != 0) {
    // printk("  %d: %s\n", i++, name);
    // attempting to go above the current root is a nop
    if (!strcmp(name, "..") && ino == uroot) {
      continue;
    }

    auto ent = ino->get_direntry(name);
    if (ent == nullptr) return -ENOENT;
    auto next = ent->ino;
    assert(next != nullptr);


    if (last) {
      if (next->is_dir()) {
        return -EISDIR;
      }

      assert(ino != nullptr);
      if (!ino->is_dir()) {
        return -ENOTDIR;
      }
      return ino->unlink(name);
    }


    ino = next;
  }

  return 0;
}


fs::File vfs::fdopen(ck::string path, int opts, int mode) {
  int fd_dirs = 0;

  if (opts & O_RDWR) fd_dirs |= FDIR_READ | FDIR_WRITE;
  if (opts & O_WRONLY) fd_dirs |= FDIR_WRITE;
  if (opts & O_RDONLY) fd_dirs |= FDIR_READ;

  auto ino = vfs::open(move(path), opts, mode);

  fs::File fd(ino, fd_dirs);

  fd.path = move(path);
  return fd;
}

ksh_def("pwd", "Print the working directory") {
  ck::string cwd;

  vfs::getcwd(*curproc->cwd, cwd);
  printk("%s\n", cwd.get());

  return 0;
}

ksh_def("ls", "List the files in the kernel's working directory") {
  auto cwd = curproc->cwd;
  {
    auto l = cwd->lock();
    auto ents = cwd->dirents();

    for (auto ent : ents) {
      printk("- %s\n", ent->name.get());
    }
  }

  return 0;
}

extern int do_chdir(const char *path);

ksh_def("cd", "Change directory") {
	if (args.size() != 1) {
		printk("usage: cd <dir>\n");
		return 0;
	}
	return do_chdir(args[0].get());
}

ksh_def("mkdir", "Create a directory") {
	if (args.size() != 1) {
		printk("usage: mkdir <dir>\n");
		return 0;
	}
	sys::mkdir(args[0].get(), 0755);
  return 0;
}
