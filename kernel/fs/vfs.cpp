#include <cpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <map.h>
#include <syscall.h>
#include <util.h>

struct fs::inode *vfs_root = NULL;
static vec<struct fs::sb_information *> filesystems;
static vec<struct vfs::mountpoint *> mountpoints;

void vfs::register_filesystem(struct fs::sb_information &info) {
  printk(KERN_INFO "filesystem '%s' registered\n", info.name);
  filesystems.push(&info);
}

void vfs::deregister_filesystem(struct fs::sb_information &) {}

struct fs::inode *vfs::cwd(void) {
  if (cpu::in_thread()) {
    auto proc = cpu::proc();

    if (proc->cwd != NULL) {
      return proc->cwd;
    }
  }

  return vfs_root;
}

struct fs::inode *vfs::get_root(void) {
  return vfs_root;
}

int vfs::mount_root(const char *src, const char *type) {
  panic("mounting root %s with type %s\n", src, type);
  return vfs::mount(src, "/", type, 0, 0);
}

int vfs::mount(const char *src, const char *targ, const char *type,
               unsigned long flags, const char *options) {
  // printk(KERN_INFO "mount %s with fs %s to %s\n", src, type, targ);

  if (get_root() == NULL && strcmp(targ, "/") != 0) {
    printk(KERN_WARN
           "Mounting non-root filesystem when there is no root is invalid");
    return -EINVAL;
  }

  if (type == NULL) return -EINVAL;

  if (get_root() != NULL) {
    // TODO: look up the target directory
  }

  struct fs::sb_information *fs = nullptr;

  for (int i = 0; i < filesystems.size(); i++) {
    if (strcmp(type, filesystems[i]->name) == 0) {
      fs = filesystems[i];
      break;
    }
  }

  if (fs == nullptr) {
    printk("failed to find the filesystem for that name\n");
    return -ENOENT;
  }
  auto *sb = fs->mount(fs, options, flags, src);
  if (sb == NULL) {
    printk("failed to mount filesystem\n");
    return -EINVAL;
  }

  auto mp = new vfs::mountpoint();
  mp->sb = sb;
  mp->mountflags = flags;
  mp->devname = targ;
  mp->id = 0;

  assert(sb->root != NULL);

  assert(&mp->sb->root->sb == sb);

  if (get_root() == NULL && strcmp(targ, "/") == 0) {
    // update the root
    // TODO: this needs to be smarter :)
    vfs_root = fs::inode::acquire(mp->sb->root);
  } else {
    // this is so gross...
    mp->host = vfs::open(targ, O_RDONLY, 0);
    if (mp->host == NULL || mp->host->type != T_DIR || mp->host == vfs_root) {
      delete mp;
      return -EINVAL;
    }
    auto parent = mp->host->get_direntry("..");
    assert(parent != NULL);
    mp->sb->root->add_mount("..", parent);

    // copy the last entry into the name of the guest root
    auto end = string(targ).split('/').last();
    auto name = malloc<char>(end.size() + 1);
    memcpy(name, end.get(), end.size() + 1);
    mp->sb->root->dir.name = name;

    // and mount it with a shadow node
    parent->add_mount(end.get(), mp->sb->root);
  }

  mountpoints.push(mp);
  return 0;
}

struct fs::inode *vfs::open(string spath, int opts, int mode) {
  struct fs::inode *ino = NULL;

  if (!cpu::in_thread()) {
    printk("not in thread\n");
  }

  if (0 != vfs::namei(spath.get(), opts, mode, vfs::cwd(), ino)) {
    return NULL;
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

  while (*path == '/') path++;
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
  while (*path == '/') path++;
  return path;
}

int vfs::namei(const char *path, int flags, int mode, struct fs::inode *cwd,
               struct fs::inode *&res) {
  assert(path != NULL);
  auto ino = cwd;

  auto uroot = curproc->root ?: vfs::get_root();
  /* if the path is rooted (/ at start), set the "working cwd" to the root
   * directory */
  if (path[0] == '/') {
    ino = uroot;
  } else {
    assert(cwd != NULL);
  }

  char name[256];
  bool last = false;

  // printk("namei %s\n", path);
  // hexdump((void*)path, strlen(path), true);

  // int i = 0;
  while ((path = skipelem((char *)path, name, last)) != 0) {
    // printk("  %d: %s\n", i++, name);
    // attempting to go above the current root is a nop
    if (!strcmp(name, "..") && ino == uroot) {
      continue;
    }

    auto found = ino->get_direntry(name);

    if (found == NULL) {
      if (last && (flags & O_CREAT)) {
        if (ino->dops && ino->dops->create) {
          fs::file_ownership own;
          own.uid = curproc->user.uid;
          own.gid = curproc->user.gid;
          own.mode = mode;
          int r = ino->dops->create(*ino, name, own);
          if (r == 0) {
            res = ino->get_direntry(name);
          }
          return r;
        }
        return -EINVAL;
      }

      res = NULL;
      return -ENOENT;
    }

    ino = found;
  }

  res = ino;
  return 0;
}



int vfs::unlink(const char *path, struct fs::inode *cwd) {
  auto ino = cwd;

  auto uroot = curproc->root ?: vfs::get_root();
  if (path[0] == '/') {
    ino = uroot;
  } else {
    assert(cwd != NULL);
  }

  char name[256];
  bool last = false;

  while ((path = skipelem((char *)path, name, last)) != 0) {
    // printk("  %d: %s\n", i++, name);
    // attempting to go above the current root is a nop
    if (!strcmp(name, "..") && ino == uroot) {
      continue;
    }

    auto next = ino->get_direntry(name);
		if (next == NULL) return -ENOENT;

    if (last) {
      if (next->type == T_DIR) {
        return -EISDIR;
      }

      assert(ino != NULL);
      if (ino->type != T_DIR) {
        return -ENOTDIR;
      }
      if (ino->dops->unlink == NULL) return -EPERM;
      return ino->dops->unlink(*ino, name);
    }


    ino = next;
  }

  return 0;
}


fs::file vfs::fdopen(string path, int opts, int mode) {
  int fd_dirs = 0;

  if (opts & O_RDWR) fd_dirs |= FDIR_READ | FDIR_WRITE;
  if (opts & O_WRONLY) fd_dirs |= FDIR_WRITE;
  if (opts & O_RDONLY) fd_dirs |= FDIR_READ;

  auto *ino = vfs::open(move(path), opts, mode);

  fs::file fd(ino, fd_dirs);

  fd.path = move(path);
  return fd;
}
