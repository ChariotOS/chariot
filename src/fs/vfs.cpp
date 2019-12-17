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

int vfs::mount(unique_ptr<fs::filesystem> fs, string host) {
  return -1;
}




struct fs::inode *vfs::open(string spath, int opts, int mode) {
  fs::path p = spath;

  struct fs::inode *curr = 0;

  if (!p.is_root()) {
    // TODO: use the current processes' CWD as root, and walk from there.
    panic("fs::vfs::open should always receive a rooted path. '%s'\n",
          spath.get());
  } else {
    curr = vfs_root;
  }

  // start out assuming we've found the file, for `open("/", ...);` cases
  bool found = true;

  for (int i = 0; i < p.len(); i++) {
    // if we are on the last directory of the path
    bool last = i == p.len() - 1;

    auto &targ = p[i];

    auto found = curr->get_direntry(targ);
    curr = found;
    if (found == NULL) {
      if (last && (opts & O_CREAT)) {
        // TODO: check for O_CREAT and last
        printk("NOT FOUND AT LAST, WOULD CREATE\n");
      }
      return {};
    }
  }

  // if the file wasnt found, return a null inode
  if (!found) return {};

  return curr;
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
