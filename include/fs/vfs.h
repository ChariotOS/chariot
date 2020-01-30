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

  /*
   * open a a file, rooted at task_process::cwd() if the path is not rooted at
   * `/`, and return the inode pointer. NULL on failure to open
   */
  static fs::inode *open(string path, int opts = 0, int mode = 0000);

  /*
   * namei()
   *
   * main "low level" interface for opening a file. Must have all arguments
   * passed.
   */
  static int namei(const char *path, int flags, int mode, struct fs::inode *cwd,
                   struct fs::inode *&res);

  /*
   * cwd()
   *
   * return the current working directory for whatever task we are in. If
   * we are not in a task, use vfs_root (/)
   */
  static struct fs::inode *cwd(void);

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

