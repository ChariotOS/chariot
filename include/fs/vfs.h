#pragma once

#include <fs/filesystem.h>
#include <ptr.h>
#include <string.h>

/**
 *
 * The virtual file system allows filesystems to be mounted recursively in the
 * directory tree. It is the main API for generalized filesystem access
 */

class vfs {
 public:

  // mount a filesystem into a host vnode
  static int mount(unique_ptr<fs::filesystem>, string path);

  static int mount_root(unique_ptr<fs::filesystem>);

  static fs::inode *open(string path, int opts = 0, int mode = 0000);

  // a mounter creates a filesystem on a device and returns it
  using mounter_t = unique_ptr<fs::filesystem> (*)(ref<dev::device>, int flags);

  // register filesystems by name, therefore we can mount generically by name
  static int register_filesystem(string, mounter_t);
  static int deregister_filesystem(string);

  static int mount(ref<dev::device>, string fs_name, string path);

  static fs::filedesc fdopen(string path, int opts = 0, int mode = 0000);

 private:
  vfs();  // private constructor. use static methods
};

