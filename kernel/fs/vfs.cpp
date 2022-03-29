#include <cpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <ck/map.h>
#include <syscall.h>
#include <module.h>
#include <util.h>
#include <fs/tmpfs.h>
#include <fs/devfs.h>
#include <thread.h>

#include <crypto.h>

#define LOG(...) PFXLOG(GRN "VFS", __VA_ARGS__)

ck::ref<fs::Node> vfs_root = nullptr;
// static ck::vec<struct fs::SuperBlockInfo *> filesystems;
static ck::vec<struct vfs::mountpoint *> mountpoints;

static ck::map<ck::string, vfs::Mounter> filesystem_mounters;

void vfs::register_filesystem(ck::string name, vfs::Mounter mount) {
  LOG("filesystem '%s' registered\n", name.get());
  filesystem_mounters[name] = mount;
}

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
  // printf(KERN_INFO "mount %s with fs %s to %s\n", src, type, targ);

  if (get_root().is_null() && strcmp(targ, "/") != 0) {
    LOG("[WARN] Mounting non-root filesystem when there is no root is invalid");
    return -EINVAL;
  }

  if (type == NULL) return -EINVAL;



  auto mounter = filesystem_mounters[type];
  if (mounter == nullptr) {
    LOG("failed to find the filesystem, '%s'\n", type);
    return -ENOENT;
  }
  auto fs = mounter(options, flags, src);
  if (fs == nullptr) {
    LOG("failed to mount filesystem\n");
    return -EINVAL;
  }

  auto mp = new vfs::mountpoint();
  mp->sb = fs;
  mp->mountflags = flags;
  mp->devname = targ;
  mp->id = 0;

  assert(fs->root);

  assert(mp->sb->root->sb == fs);

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
    ino = found->get();
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
    // printf("  %d: %s\n", i++, name);
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


int vfs::chroot(ck::string path) {
  auto proc = curproc;
  scoped_lock l(proc->datalock);

  ck::ref<fs::Node> new_root = nullptr;

  if (0 != vfs::namei(path.get(), 0, 0, proc->cwd, new_root)) return -1;

  if (new_root == nullptr) return -ENOENT;

  if (!new_root->is_dir()) return -ENOTDIR;

  proc->root = new_root;

  ck::string cwd;
  if (vfs::getcwd(*new_root, cwd) != 0) return -EINVAL;
  proc->cwd_string = cwd;
  proc->cwd = new_root;

  if (curproc->ring == RING_KERN) {
    vfs_root = new_root;
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




void vfs::init_boot_filesystem(void) {
  // Initialize the tmpfs filesystem layer
  tmpfs::init();
  devfs::init();

  int mount_res;
  // Mount a tmpfs filesystem at "/"
  mount_res = vfs::mount("", "/", "tmpfs", 0, NULL);
  if (mount_res != 0) panic("Failed to mount root tmpfs filesystem\n");
  sys::mkdir("/dev", 0755);
  mount_res = vfs::mount("", "/dev", "devfs", 0, NULL);
  if (mount_res != 0) panic("Failed to mount devfs to /dev\n");
  // Once the kernel has a drive and a filesystem for the root filesystem, it
  // is mounted into /uroot, and then chrooted into
  sys::mkdir("/root", 0755);
}


ksh_def("pwd", "Print the working directory") {
  ck::string cwd;

  vfs::getcwd(*curproc->cwd, cwd);
  printf("%s\n", cwd.get());

  return 0;
}

static void do_ls(const ck::string &dir) {
  auto cwd = curproc->cwd;
  auto f = vfs::open(dir);
  if (!f) {
    printf("ls: '%s' not found\n", dir.get());
    return;
  }

  {
    auto l = f->lock();
    auto ents = f->dirents();

    printf("%8s ", "iNode");
    printf("%8s ", "Mode");
    printf("%8s ", "Links");
    printf("%12s ", "Bytes");
    printf("%12s ", "Block Count");
    printf("%12s ", "Block Size");
    printf("%s", "Name");
    printf("\n");

    for (auto ent : ents) {
      auto n = ent->get();
      struct stat s;
      n->stat(&s);

      printf("%8ld ", s.st_ino);
      printf("%8lo ", s.st_mode);
      printf("%8ld ", s.st_nlink);
      printf("%12ld ", s.st_size);
      printf("%12ld ", s.st_blocks);
      printf("%12ld ", s.st_blksize);
      printf("%s", ent->name.get());
      printf("\n");
      // printf("%zu %s\n", n->size(), ent->name.get());
    }
  }
}

ksh_def("ls", "List the files in the kernel's working directory") {
  if (args.size() == 0) {
    do_ls(".");
  } else {
    for (auto &arg : args) {
      do_ls(arg);
    }
  }
  return 0;
}

extern int do_chdir(const char *path);

ksh_def("cd", "Change directory") {
  if (args.size() != 1) {
    printf("usage: cd <dir>\n");
    return 0;
  }
  return do_chdir(args[0].get());
}

ksh_def("mkdir", "Create a directory") {
  if (args.size() != 1) {
    printf("usage: mkdir <dir>\n");
    return 0;
  }
  sys::mkdir(args[0].get(), 0755);
  return 0;
}


ksh_def("dump", "hexdump the start of a file") {
  if (args.size() != 1) {
    printf("usage: dump file\n");
    return 0;
  }
  auto buf = (char *)malloc(4096);
  auto f = vfs::open(args[0]);
  if (!f) {
    printf("file not found\n");
    free(buf);
    return 0;
  }

  fs::File file(f, 0);
  auto sz = file.read(buf, 4096);
  if (sz < 0) {
    printf("failed to read: %zd\n", sz);
    free(buf);
    return 0;
  }
  printf("read %zd bytes\n", sz);
  hexdump(buf, sz, true);
  free(buf);
  return 0;
}


ksh_def("md5", "md5sum a file") {
  if (args.size() != 1) {
    printf("usage: md5 <file>\n");
    return 0;
  }

  auto f = vfs::open(args[0]);
  if (!f) {
    printf("file not found\n");
    return 0;
  }

  fs::File file(f, 0);

  int size = 8192 * 4;
  auto buf = (char *)malloc(size);
  crypto::MD5 sum;
  struct stat s;
  file.stat(&s);

  size_t read = 0;
  while (true) {
    auto sz = file.read(buf, size);
    if (sz < 0) {
      printf("failed to read: %zd\n", sz);
      free(buf);
      return 0;
    }
    if (sz == 0) {
      printf("\n");
      break;
    }

    read += sz;
    printf("\r%zu", (read * 100) / s.st_size);
    file.seek(sz, SEEK_CUR);

    // sum.update(buf, sz);
  }
  free(buf);
  auto digest = sum.finalize();
  printf("%s\n", digest.get());
  // hexdump(buf, sz, true);
  return 0;
}
