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
	struct vmobject;
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

  struct block_operations {
    // used to populate block_count and block_size
    int (&init)(fs::blkdev &);
    int (*open)(fs::blkdev &, int mode);
    int (*release)(fs::blkdev &);
    // 0 on success, -ERRNO on fail
    int (&rw_block)(fs::blkdev &, void *data, int block, bool write);
    int (*ioctl)(fs::blkdev &, unsigned int, off_t) = NULL;
  };

  struct blkdev {
    dev_t dev;
    string name;  // ex. hda or ata0

    size_t block_count;
    size_t block_size;

    struct block_operations &ops;

    long count = 0;  // refcount
    spinlock lock;

    inline blkdev(dev_t dev, string name, struct block_operations &ops)
        : dev(dev), name(move(name)), ops(ops) {
      ops.init(*this);
    }

    // nice wrappers for filesystems and all that :)
    inline int read_block(void *data, int block) {
      return ops.rw_block(*this, data, block, false);
    }

    inline int write_block(void *data, int block) {
      return ops.rw_block(*this, data, block, true);
    }

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

    // nice to have thing
    template <typename T>
    T *&p(void) {
      return (T *&)priv;
    }

   private:
    void *priv;
  };

  // used in the kernel to look up "/dev/*" block devices before we have /dev
  // (and really, the kernel cannot rely on userspace filesystems in the end
  // either. implemented in kernel/dev/driver.cpp (where we have the driver name
  // info)
  struct fs::blkdev *bdev_from_path(const char *);


  extern struct superblock DUMMY_SB;

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
      if (s->count == 0 && s != &DUMMY_SB) {
        printk("s->count == 0 now. Must delete!\n");
      }
      s->lock.write_unlock();
    }

   private:
    void *priv;
  };


  struct sb_operations {
    // initialize the superblock after it has been mounted. All of the
    // arguments
    int (&init)(struct superblock &);
    int (&write_super)(struct superblock &);
    int (&sync)(struct superblock &, int flags);
  };

  struct sb_information {
    const char *name;

    // return a superblock containing the root inode
    struct fs::superblock *(&mount)(struct sb_information *, const char *args,
                                    int flags, const char *device);

    struct sb_operations &ops;
  };

  struct inode *bdev_mount(struct sb_information *info, const char *args,
                           int flags);

  struct inode *open(const char *s, u32 flags, u32 opts = 0);

  ref<fs::file> bdev_to_file(fs::blkdev *);

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
    int nr = -1;  // for unresolved
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
		ref<mm::vmobject> (*mmap)(fs::file &, size_t npages, int prot, int flags, off_t off);
    // resize a file. if size is zero, it is a truncate
    int (*resize)(fs::file &, size_t);

    void (*destroy)(fs::inode &);

		// *quickly* poll for a bitmap of events and return a bitmap of those that match
		int (*poll)(fs::file &, int events);
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
  };



  /**
   * struct inode - base point for all "file-like" objects
   *
   * RULE: if an inode has a pointer to another inode inside of it, it owns it.
   *       this means you cannot have circular references - hardlinks to one
   * file in two different dirs must have different `struct inode`s
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
      uint16_t major, minor;
    } dev;
    uint32_t ino = 0;  // inode (in systems that support it)
    uint16_t uid = 0;
    uint16_t gid = 0;
    uint32_t link_count = 0;
    uint32_t atime = 0;
    uint32_t ctime = 0;
    uint32_t mtime = 0;
    uint32_t dtime = 0;
    uint16_t block_size = 0;

    // for devices
    int major, minor;

    fs::file_operations *fops = NULL;
    fs::dir_operations *dops = NULL;

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

      // T_BLK
      struct {
        fs::blkdev *dev;
      } blk;
    };


		// if this inode has a socket bound to it, it will be located here.
    struct net::sock *sk = NULL;
    struct net::sock *bound_socket = NULL;

    /*
     * the directory entry list in this->as.dir is a linked list that must be
     * traversed to find the entry. When the subclass creates a directory, it
     * must populate that list with at least the names of the entries. If an
     * entry contains a NULL inode, it calls 'resolve_direntry' which returns
     * the backing inode.
     */
    int register_direntry(string name, int type, int nr, struct inode * = NULL);
    int remove_direntry(string name);
    struct inode *get_direntry(const char *name);
    int get_direntry_r(const char *name);
    void walk_direntries(func<bool(const string &, struct inode *)>);
    vec<string> direntries(void);
    int add_mount(const char *name, struct inode *other_root);

    struct direntry *get_direntry_raw(const char *name);

		int poll(fs::file &, int events);

    // if the inode is a directory, set its name. NOP otherwise
    int set_name(const string &);

    inode(int type, fs::superblock &sb);
    virtual ~inode();

    int stat(struct stat *);

    static fs::inode *acquire(struct inode *);
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
    int m_error = 0;

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
    int ioctl(int cmd, unsigned long arg);

    int close();

    inline off_t offset(void) { return m_offset; }

    fs::file_operations *fops(void);

    inline int errorcode() { return m_error; };

    ~file(void);


    file(struct fs::inode *, int flags);
    inline file() : file(NULL, 0) {}

    inline operator bool() { return ino != NULL; }

    struct fs::inode *ino;
    string path;
    off_t m_offset = 0;

		bool can_write = false;
		bool can_read = false;

		int pflags = 0; // private flags
  };

};  // namespace fs

static inline fs::inode *geti(fs::inode *i) { return fs::inode::acquire(i); }



// fwd decl
namespace mm {
  struct page;
};

/**
 * functions to interact with a block devices and the block cache
 */
namespace block {

  // a buffer represents a page (4k) in a block device.
  struct buffer {
    fs::blkdev &bdev; /* the device this buffer belongs to */

    static struct buffer *get(fs::blkdev &, off_t page);

    static void release(struct buffer *);

    void register_write(void);

		ref<mm::page> page(void);

    // return the backing page data.
    void *data(void);
    off_t index(void);
    int flush(void);
    inline uint64_t last_used(void) { return m_last_used; }
    inline auto owners(void) {
      return m_count;  // XXX: race condition (!?!?)
    }

		size_t reclaim(void);

   protected:
    inline static void release(struct blkdev *d) {}

    buffer(fs::blkdev &, off_t);

    bool m_dirty = false;
    spinlock m_lock;
    int m_size = PGSIZE;
    uint64_t m_count = 0;
    off_t m_index;
    uint64_t m_last_used = 0;
    ref<mm::page> m_page;
  };


	size_t reclaim_memory(void);

};  // namespace block


// read data from blocks to from a byte offset. These can be somewhat wasteful,
// but that gets amortized by the block flush daemon :^)
int bread(fs::blkdev &, void *dst, size_t size, off_t byte_offset);
int bwrite(fs::blkdev &, void *data, size_t size, off_t byte_offset);

// reclaim some memory

static inline auto bget(fs::blkdev &b, off_t page) {
  return block::buffer::get(b, page);
}

// release a block
static inline auto bput(struct block::buffer *b) {
  return block::buffer::release(b);
}


struct bref {
	static inline bref get(fs::blkdev &b, off_t page)  {
		return block::buffer::get(b, page);
	}

	inline bref(struct block::buffer *b) {
		buf = b;
	}

	inline ~bref(void) {
		if (buf) bput(buf);
	}

	inline struct block::buffer *operator->(void) {
		return buf;
	}

	private:
	struct block::buffer *buf = NULL;
};

#endif
