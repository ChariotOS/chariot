#include <atom.h>
#include <func.h>
#include <lock.h>
#include <string.h>
#include <types.h>

namespace fs {
// fwd decl
struct inode;

// memory only
#define ENT_MEM 0
// resident on the backing storage
//   if the mode is this, the subclass will be asked if it is okay to remove the
//   node and handle it itself
#define ENT_RES 1

// directories have entries (linked list)
struct direntry {
  int type = ENT_RES;
  string name;
  struct inode *ino;

  // if this direntry is a mount, this inode will shadow it
  struct inode *mount_shadow = NULL;

  struct direntry *next, *prev;
};

#define T_INVA 0
#define T_DIR 1
#define T_FILE 2

// TODO:
#define T_PIPE 3
#define T_CHAR 4
#define T_BLK 5
#define T_SYML 6
#define T_SOCK 7

/**
 * struct inode - base point for all "file-like" objects
 *
 * RULE: if an inode has a pointer to another inode inside of it, it owns it.
 *       this means you cannot have circular references - hardlinks to one file
 *       in two different dirs must have different `struct inode`s
 */

struct inode {
  /**
   * fields
   */
  off_t size = 0;
  int type = T_INVA;  // from T_[...] above

  u32 ino = 0;  // inode (in systems that support it)
  u32 uid = 0;
  u32 gid = 0;
  u32 link_count = 0;
  time_t atime = 0;
  time_t ctime = 0;
  time_t mtime = 0;
  time_t dtime = 0;
  u32 block_count = 0;
  u32 block_size = 0;

  // for devices
  int major, minor;

  // for inline linked list inside the filesystem itself
  struct inode *next, *prev;

  // different kinds of inodes need different kinds of data
  union {
    struct {
      // the parent directory, if this node is mounted
      struct inode *mountpoint;

      // directories can have other filesystems mounted in them, so `mounts`
      // holds any of those mounts in it. If a mount exists by a certain name,
      // and another directory in this dir has the same name, the mount is given
      struct direntry *entries;
    } dir;
  };

  /**
   * methods
   */

  inode(int type);

  // destructs (and deletes) all inodes this inode owns
  virtual ~inode();

  // getter functions
  inline bool is_dir() { return type == T_DIR; }
  inline bool is_file() { return type == T_FILE; }
  inline bool is_pipe() { return type == T_PIPE; }
  inline bool is_chardev() { return type == T_CHAR; }
  inline bool is_blkdev() { return type == T_BLK; }
  inline bool is_symlink() { return type == T_SYML; }
  inline bool is_sock() { return type == T_SOCK; }

  // file descriptors call this when opening or closing this inode
  inline void fd_open() { n_open.store(n_open.load() + 1); }
  inline void fd_close() { n_open.store(n_open.load() - 1); }

  /*
   * the directory entry list in this->as.dir is a linked list that must be
   * traversed to find the entry. When the subclass creates a directory, it must
   * populate that list with at least the names of the entries. If an entry
   * contains a NULL inode, it calls 'resolve_direntry' which returns the
   * backing inode.
   */
  int register_direntry(string name, int type, struct inode * = NULL);
  int remove_direntry(string name);

  struct inode *get_direntry(string &name);
  virtual struct inode *resolve_direntry(string &name);

  // remove an entry in the directory (only called on entry removals of type
  // ENT_RES). Returns 0 on success, <0 on failure or the file cannot be removed
  virtual int rm(string &name);

  void walk_direntries(
      func<bool /* continue? */ (const string &, struct inode *)>);

  vec<string> direntries(void);

  // add a mount in the current directory, returning 0 on success
  int add_mount(string &name, struct inode *other_root);

 private:
  mutex_lock lock;

  struct inode *get_direntry_nolock(string &name);
  struct inode *get_direntry_ino(struct direntry *);
  // how many file descriptors have this inode open (good for keeping track of
  // busy-ness when unmounting a filesystem)
  atom<int> n_open = 0;
};

};  // namespace fs
