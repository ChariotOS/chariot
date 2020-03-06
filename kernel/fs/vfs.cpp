#include <cpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <map.h>
#include <syscall.h>
#include <util.h>

vec<unique_ptr<fs::filesystem>> mounted_filesystems;
struct fs::inode *vfs_root = NULL;

static map<string, vfs::mounter_t> filesystems;

int vfs::register_filesystem(string name, mounter_t m) {
  if (filesystems.contains(name)) {
    return -EEXIST;
  }
  KINFO("Registered filesystem '%s'\n", name.get());
  filesystems[move(name)] = m;
  return 0;
}

int vfs::deregister_filesystem(string name) {
  if (filesystems.contains(name)) {
    filesystems.remove(name);
    return 0;
  }
  return -ENOENT;
}

int vfs::mount_root(unique_ptr<fs::filesystem> fs) {
  assert(vfs_root == NULL);
  vfs_root = fs->get_root();
  vfs_root->set_name("");
  fs::inode::acquire(vfs_root);
  mounted_filesystems.push(move(fs));

  return 0;
}

int vfs::mount(ref<dev::device>, string fs_name, string path) {
  // special case for mounting the root
  if (path == "/") {
    printk("mounting root!\n");
  }
  return -ENOTIMPL;
}

vfs::vfs() { panic("DO NOT CONSTRUCT A VFS INSTANCE\n"); }

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

int vfs::mount(unique_ptr<fs::filesystem> fs, string host) { return -1; }

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
  /* if the path is rooted (/ at start), set the "working cwd" to the root
   * directory */
  if (path[0] == '/') {
    ino = vfs_root;
  } else {
    assert(cwd != NULL);
  }

  char name[256];
  bool last = false;
  while ((path = skipelem((char *)path, name, last)) != 0) {

    auto found = ino->get_direntry(name);

    if (found == NULL) {
      if (last && (flags & O_CREAT)) {

        if (ino->dops && ino->dops->create) {

          fs::file_ownership own;
          own.uid = curproc->user.uid;
          own.gid = curproc->user.gid;
          own.mode = mode;
          return ino->dops->create(*ino, name, own);
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
