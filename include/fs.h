#pragma once

#ifndef __FS__H__
#define __FS__H__

#include <atom.h>
#include <func.h>
#include <lock.h>
#include <stat.h>
#include <string.h>
#include <types.h>
#include <wait.h>

#define FDIR_READ 1
#define FDIR_WRITE 2

#define SEEK_SET (-1)
#define SEEK_CUR (-2)
#define SEEK_END (-3)

namespace fs {
// fwd decl
struct inode;

class filedesc : public refcounted<filedesc> {
 public:
  // must construct file descriptors via these factory funcs
  static ref<filedesc> create(struct fs::inode *, string open_path,
                              int flags = FDIR_READ | FDIR_WRITE);

  /*
   * seek - change the offset
   * set the file position to the offset + whence if whence is not equal to
   * either SEEK_SET, SEEK_CUR, or SEEK_END. (defaults to start)
   */
  off_t seek(off_t offset, int whence = SEEK_SET);
  ssize_t read(void *, ssize_t);
  ssize_t write(void *data, ssize_t);

  int close();

  inline off_t offset(void) { return m_offset; }

  ~filedesc(void);

 public:
  filedesc(struct fs::inode *, int flags);
  inline filedesc() : filedesc(NULL, 0) {}

  inline operator bool() { return ino != NULL; }

  struct fs::inode *ino;
  string path;
  off_t m_offset = 0;
};

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
#define T_FIFO 3
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
  short type = T_INVA;  // from T_[...] above
  short mode = 0;       // file mode. ex: o755
  uint32_t ino = 0;     // inode (in systems that support it)
  uint32_t uid = 0;
  uint32_t gid = 0;
  uint32_t link_count = 0;
  uint32_t atime = 0;
  uint32_t ctime = 0;
  uint32_t mtime = 0;
  uint32_t dtime = 0;
  uint32_t block_size = 0;

  // for devices
  int major, minor;

  // different kinds of inodes need different kinds of data
  union {
    struct {
      // the parent directory, if this node is mounted
      struct inode *mountpoint;
      struct direntry *entries;
    } dir;
  };

  /**
   * methods
   */

  inode(int type);

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

  void walk_direntries(
      func<bool /* continue? */ (const string &, struct inode *)>);

  vec<string> direntries(void);

  // add a mount in the current directory, returning 0 on success
  int add_mount(string &name, struct inode *other_root);

  /**
   * file related functions
   */
  ssize_t read(filedesc &, void *, size_t);
  ssize_t write(filedesc &, void *, size_t);

  int open(filedesc &);
  void close(filedesc &);

  /**
   * virtual functions
   */
  // destructs (and deletes) all inodes this inode owns
  virtual ~inode();
  virtual struct inode *resolve_direntry(string &name);
  // remove an entry in the directory (only called on entry removals of type
  // ENT_RES). Returns 0 on success, <0 on failure or the file cannot be removed
  virtual int rm(string &name);
  virtual ssize_t do_read(filedesc &, void *, size_t);
  virtual ssize_t do_write(filedesc &, void *, size_t);

  // can overload!
  virtual int stat(struct stat *);

  static int acquire(struct inode *);
  static int release(struct inode *);

 protected:
  spinlock lock = spinlock("inode lock");
  int rc = 0;

 private:
  struct inode *get_direntry_nolock(string &name);
  struct inode *get_direntry_ino(struct direntry *);
};

// src/fs/pipe.cpp
struct pipe : public fs::inode {
  // uid and gid are the creators of this pipe

  waitqueue wq;

  unsigned int readers;
  unsigned int writers;

  uint8_t *data = NULL;

  uint32_t write_ind;
  uint32_t read_ind;
  uint32_t capacity;
  uint32_t used_bytes;

  // ctor
  pipe();
  virtual ~pipe(void);

  // main interface
  virtual ssize_t do_read(filedesc &, void *, size_t);
  virtual ssize_t do_write(filedesc &, void *, size_t);
};

};  // namespace fs

#endif
