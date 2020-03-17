#pragma once

#ifndef __FS__H__
#define __FS__H__

#include <atom.h>
#include <dev/device.h>
#include <func.h>
#include <lock.h>
#include <net/sock.h>
#include <stat.h>
#include <string.h>
#include <types.h>
#include <wait.h>

#define FDIR_READ 1
#define FDIR_WRITE 2

#define SEEK_SET (-1)
#define SEEK_CUR (-2)
#define SEEK_END (-3)

// fwd decl
namespace mm {
struct area;
}

namespace fs {
// fwd decl
struct inode;
struct blkdev;
class file;

using mode_t = u32;

struct directory_entry {
  u32 inode;
  string name;
};

class filesystem {
 public:
  virtual ~filesystem();

  virtual bool init(void) = 0;
  virtual struct inode *get_root() = 0;
  inline virtual bool umount(void) { return true; }

 protected:
  // TODO: put a lock in here.

  // must be called by subclasses
  filesystem();
};




struct block_operations {
  int (*open)(fs::blkdev &, int mode);
  int (*release)(fs::blkdev &);
  // 0 on success, -ERRNO on fail
  int (&rw_block)(fs::blkdev &, void *data, int block, bool write);
  int (*ioctl)(fs::blkdev &, unsigned int, off_t) = NULL;
};


struct blkdev {
  dev_t dev;
  string name; // ex. hda or ata0

  size_t block_count;
  size_t block_size;

  struct block_operations &ops;

  long count = 0; // refcount
  spinlock lock;

  inline static void acquire(struct blkdev *d) {
    d->lock.lock();
    d->count++;
    d->lock.unlock();
  }
  inline static void release(struct blkdev *d) {
    d->lock.lock();
    d->count--;
    if (d->count == 0) {
      printk("delete blkdev [%d,%d]\n", d->dev.major(), d->dev.minor());
    }
    d->lock.unlock();
  }
};



struct superblock {
  //
  dev_t dev;
  long block_size;
  long max_filesize;
  struct sb_operations *ops;
  struct sb_information *info;
  struct inode *root;
  string arguments;

  rwlock lock;
  long count = 0;  // refcount

  // nice to have thing
  template <typename T>
  T *&p(void) {
    return (T *&)priv;
  }

  inline static void get(struct superblock *s) {
    s->lock.write_lock();
    s->count++;
    s->lock.write_unlock();
  }

  inline static void put(struct superblock *s) {
    s->lock.write_lock();
    s->count--;
    if (s->count == 0) {
      printk("s->count == 0 now. Must delete!\n");
    }
    s->lock.write_unlock();
  }

 private:
  void *priv;
};


extern struct superblock DUMMY_SB;

struct sb_operations {
  // initialize the superblock after it has been mounted. All of the
  // arguments
  int (&init)(struct superblock &);
  int (&write_super)(struct superblock &);
  int (&sync)(struct superblock &, int flags);
};

struct sb_information {
  const char *name;

  // mount and return the root inode, returning NULL on failure.
  struct inode *(*mount)(struct sb_information *, const char *args, int flags,
                         const char *device);

  struct sb_operations &ops;
};

// typically
struct inode *bdev_mount(struct sb_information *info, const char *args,
                         int flags);

int mount(string path, filesystem &);

struct inode *open(string s, u32 flags, u32 opts = 0);

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

struct file_ownership {
  int uid;
  int gid;
  int mode;  // the octal part of a file :)
};

struct file_operations {
  // seek - used to notify of seeking. Doesn't actually seek
  //        returns -errno on failure (seek wasn't allowed)
  int (*seek)(fs::file &, off_t old_off, off_t new_off) = NULL;
  ssize_t (*read)(fs::file &, char *, size_t) = NULL;
  ssize_t (*write)(fs::file &, const char *, size_t) = NULL;
  int (*ioctl)(fs::file &, unsigned int, off_t) = NULL;

  /**
   * open - notify the driver that a new descriptor has opened the file
   * if it returns a non-zero code, it will be propagated back to the
   * sys::open() call and be considered a fail
   */
  int (*open)(fs::file &) = NULL;

  /**
   * close - notify the driver that a file descriptor is closing it
   *
   * no return value
   */
  void (*close)(fs::file &) = NULL;

  /* map a file into a vm area */
  int (*mmap)(fs::file &, struct mm::area &);
  // resize a file. if size is zero, it is a truncate
  int (*resize)(fs::file &, size_t);

  void (*destroy)(fs::inode &);
};


// wrapper functions for block devices
extern struct fs::file_operations block_file_ops;

struct dir_operations {
  // create a file in the directory
  int (*create)(fs::inode &, const char *name, struct fs::file_ownership &);
  // create a directory in a dir
  int (*mkdir)(fs::inode &, const char *, struct fs::file_ownership &);
  // remove a file from a directory
  int (*unlink)(fs::inode &, const char *);
  // lookup an inode by name in a file
  struct fs::inode *(*lookup)(fs::inode &, const char *);
  // create a device node with a major and minor number
  int (*mknod)(fs::inode &, const char *name, struct fs::file_ownership &,
               int major, int minor);

  // walk through the directory, calling the callback per entry
  int (*walk)(fs::inode &, func<bool(const string &)>);
};

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

  // the device that the file is located on
  struct {
    int major, minor;
  } dev;
  uint32_t ino = 0;  // inode (in systems that support it)
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

  fs::file_operations *fops = NULL;
  fs::dir_operations *dops = NULL;
  // the filesystem that this inode uses
  fs::filesystem *fs;

  fs::superblock &sb;

  void *_priv;

  bool fixed = false;

  template <typename T>
  T *&priv(void) {
    return (T *&)_priv;
  }

  // different kinds of inodes need different kinds of data
  union {
    // T_DIR
    struct {
      // the parent directory, if this node is mounted
      struct inode *mountpoint;
      struct direntry *entries;
      // an "owned" string that represents the name
      const char *name;
    } dir;

    // T_SOCK
    struct net::sock *sk;


    struct {
      fs::blkdev *dev;
    } blk;
  };

  /*
   * the directory entry list in this->as.dir is a linked list that must be
   * traversed to find the entry. When the subclass creates a directory, it must
   * populate that list with at least the names of the entries. If an entry
   * contains a NULL inode, it calls 'resolve_direntry' which returns the
   * backing inode.
   */
  int register_direntry(string name, int type, struct inode * = NULL);
  int remove_direntry(string name);
  struct inode *get_direntry(const char *name);
  void walk_direntries(func<bool(const string &, struct inode *)>);
  vec<string> direntries(void);
  int add_mount(string &name, struct inode *other_root);

  // if the inode is a directory, set its name. NOP otherwise
  int set_name(const string &);

  inode(int type, fs::superblock &sb);
  virtual ~inode();

  int stat(struct stat *);

  static int acquire(struct inode *);
  static int release(struct inode *);

  spinlock lock;

 protected:
  int rc = 0;

 private:
  struct inode *get_direntry_nolock(const char *name);
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
  virtual ssize_t do_read(file &, void *, size_t);
  virtual ssize_t do_write(file &, void *, size_t);
};





class file : public refcounted<file> {
 public:
  // must construct file descriptors via these factory funcs
  static ref<file> create(struct fs::inode *, string open_path,
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

  fs::file_operations *fops(void);

  ~file(void);

 public:
  file(struct fs::inode *, int flags);
  inline file() : file(NULL, 0) {}

  inline operator bool() { return ino != NULL; }

  struct fs::inode *ino;
  string path;
  off_t m_offset = 0;
};






};  // namespace fs

#endif
