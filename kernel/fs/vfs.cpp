#include <cpu.h>
#include <errno.h>
#include <fs/file.h>
#include <fs/vfs.h>
#include <map.h>
#include <process.h>

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

int vfs::namei(const char *path, int flags, int mode, struct fs::inode *cwd,
               struct fs::inode *&res) {
  assert(path != NULL);

  // parse the fs::path from the given path
  fs::path p = string(path);

  auto ino = cwd;
  /* if the path is rooted (/ at start), set the "working cwd" to the root
   * directory */
  if (p.is_root()) {
    // printk("rooted\n");
    ino = vfs_root;
  } else {
    // printk("not rooted\n");
    assert(cwd != NULL);
  }

  for (int i = 0; i < p.len(); i++) {
    // if we are on the last directory of the path
    bool last = i == p.len() - 1;
    string &targ = p[i];

    // printk("%d:%s\n", i, targ.get());

    auto found = ino->get_direntry(targ);

    if (found == NULL) {
      if (last && (flags & O_CREAT)) {
        // TODO: check for O_CREAT and last
        // printk("NOT FOUND AT LAST, WOULD CREATE %s %o\n", targ.get(), mode);
        return ino->touch(targ, mode, res);
      }

      res = NULL;
      return -ENOENT;
    }

    ino = found;
  }

  res = ino;
  return 0;
}

fs::filedesc vfs::fdopen(string path, int opts, int mode) {
  int fd_dirs = 0;

  if (opts & O_RDWR) fd_dirs |= FDIR_READ | FDIR_WRITE;
  if (opts & O_WRONLY) fd_dirs |= FDIR_WRITE;
  if (opts & O_RDONLY) fd_dirs |= FDIR_READ;

  auto *ino = vfs::open(move(path), opts, mode);

  fs::filedesc fd(ino, fd_dirs);

  fd.path = move(path);
  return fd;
}
