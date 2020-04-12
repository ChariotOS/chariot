#pragma once

#include <dev/device.h>
#include <fs.h>
#include <ptr.h>
#include <string.h>

/**
 *
 * The virtual file system allows filesystems to be mounted recursively in the
 * directory tree. It is the main API for generalized filesystem access
 */

class vfs {
 public:
  static int mount(const char *source, const char *targ, const char *type,
		   unsigned long /* TODO */ flags, const char *options);

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

  static struct fs::inode *get_root(void);

  // a mounter creates a filesystem on a device and returns it
  using mounter_t = unique_ptr<fs::filesystem> (*)(fs::blkdev *, int flags);

  // register filesystems by name, therefore we can mount generically by name
	static void register_filesystem(struct fs::sb_information &);
  static void deregister_filesystem(struct fs::sb_information &);

  static int mount(fs::blkdev *, string fs_name, string path);

  static fs::file fdopen(string path, int opts = 0, int mode = 0000);

  static int getcwd(fs::inode &, string &dst);

 private:
  vfs();  // private constructor. use static methods
};

