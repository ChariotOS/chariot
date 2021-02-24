#pragma once

#include <dev/device.h>
#include <fs.h>
#include <ptr.h>
#include <string.h>

namespace fs {
	// fwd decl
	struct superblock;
};

/**
 *
 * The virtual file system allows filesystems to be mounted recursively in the
 * directory tree. It is the main API for generalized filesystem access
 */

namespace vfs {

/**
 * mountpoint - describes a single filesystem mounted on the system
 */
struct mountpoint {
  struct fs::superblock *sb;

	// if this is null, it is the root node.
  struct fs::inode *host;
	int mountflags;

	string devname;

	int id; 

  struct mount *next;
  struct mount *prev;

};

// for use by the kernel only. (user cant access this function)
int mount_root(const char *source, const char *type);

// mount a device to a target with a certain filesystem
int mount(const char *source, const char *targ, const char *type,
	  unsigned long /* TODO */ flags, const char *options);

/*
 * open a a file, rooted at task_process::cwd() if the path is not rooted at
 * `/`, and return the inode pointer. NULL on failure to open
 */
fs::inode *open(string path, int opts = 0, int mode = 0000);

fs::file fdopen(string path, int opts = 0, int mode = 0000);

/*
 * namei()
 *
 * main "low level" interface for opening a file. Must have all arguments
 * passed.
 *
 * parent: stop at the parent directory of the last entry
 */
int namei(const char *path, int flags, int mode, struct fs::inode *cwd,
	  struct fs::inode *&res, bool get_last = false);


int unlink(const char *path, struct fs::inode *cwd);

/*
 * cwd()
 *
 * return the current working directory for whatever task we are in. If
 * we are not in a task, use vfs_root (/)
 */
struct fs::inode *cwd(void);
int getcwd(fs::inode &, string &dst);

struct fs::inode *get_root(void);

// register filesystems by name, therefore we can mount generically by name
void register_filesystem(struct fs::sb_information &);
void deregister_filesystem(struct fs::sb_information &);

};  // namespace vfs

