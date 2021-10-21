#pragma once

#ifndef __FS__H__
#define __FS__H__

#include <atom.h>
#include <dev/device.h>
#include <ck/func.h>
#include <lock.h>
#include <stat.h>
#include <ck/string.h>
#include <types.h>
#include <wait.h>


struct poll_table_wait_entry {
  struct wait_entry entry;
  wait_queue *wq;
  struct poll_table *table;
  short events;
  int index;
  bool en; /* idk im trying my best. */
  /* Was this the entry that was awoken? */
  bool awoken = false;
};


// poll_table is implemented in awaitfs.cpp
// struct poll_table_entry {
//   wait_queue *wq;
//   short events;
// };

struct poll_table {
  ck::vec<poll_table_wait_entry *> ents;
  int index;
  void wait(wait_queue &wq, short events);
};



#define FDIR_READ 1
#define FDIR_WRITE 2

#define SEEK_SET (-1)
#define SEEK_CUR (-2)
#define SEEK_END (-3)

// fwd decl
namespace mm {
  struct MappedRegion;
  struct VMObject;
}  // namespace mm

namespace net {
  struct Socket;
};

namespace fs {
  // fwd decl
  struct Node;
  struct BlockDevice;
  class File;

  using mode_t = u32;

  struct directory_entry {
    u32 inode;
    ck::string name;
  };

  struct BlockOperations {
    // used to populate block_count and block_size
    int (&init)(fs::BlockDevice &);
    int (*open)(fs::BlockDevice &, int mode);
    int (*release)(fs::BlockDevice &);
    // 0 on success, -ERRNO on fail
    int (&rw_block)(fs::BlockDevice &, void *data, int block, bool write);
    int (*ioctl)(fs::BlockDevice &, unsigned int, off_t) = NULL;
  };

  struct BlockDevice {
    dev_t dev;
    ck::string name;  // ex. hda or ata0

    size_t block_count;
    size_t block_size;

    struct BlockOperations &ops;

    long count = 0;  // refcount
    spinlock lock;

    inline BlockDevice(dev_t dev, ck::string name, struct BlockOperations &ops) : dev(dev), name(move(name)), ops(ops) { ops.init(*this); }

    // nice wrappers for filesystems and all that :)
    inline int read_block(void *data, int block) { return ops.rw_block(*this, data, block, false); }

    inline int write_block(void *data, int block) { return ops.rw_block(*this, data, block, true); }

    inline static void acquire(struct BlockDevice *d) {
      d->lock.lock();
      d->count++;
      d->lock.unlock();
    }
    inline static void release(struct BlockDevice *d) {
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
  struct fs::BlockDevice *bdev_from_path(const char *);



  struct SuperBlock : public ck::refcounted<SuperBlock> {
    //
    dev_t dev;
    long block_size;
    long max_filesize;

    struct SuperBlockOperations *ops;
    struct SuperBlockInfo *info;
    ck::ref<fs::Node> root;
    ck::string arguments;

    rwlock lock;

    // nice to have thing
    template <typename T>
    T *&p(void) {
      return (T *&)priv;
    }


   private:
    void *priv;
  };

  extern ck::ref<fs::SuperBlock> DUMMY_SB;



  struct SuperBlockOperations {
    // initialize the superblock after it has been mounted. All of the
    // arguments
    int (&init)(fs::SuperBlock &);
    int (&write_super)(fs::SuperBlock &);
    int (&sync)(fs::SuperBlock &, int flags);
  };

  struct SuperBlockInfo {
    const char *name;

    // return a superblock containing the root inode
    ck::ref<fs::SuperBlock> (&mount)(fs::SuperBlockInfo *, const char *args, int flags, const char *device);

    struct SuperBlockOperations &ops;
  };

  ck::ref<fs::Node> bdev_mount(fs::SuperBlockInfo *info, const char *args, int flags);

  ck::ref<fs::Node> open(const char *s, u32 flags, u32 opts = 0);

  ck::ref<fs::File> bdev_to_file(fs::BlockDevice *);

// memory only
#define ENT_MEM 0
// resident on the backing storage
//   if the mode is this, the subclass will be asked if it is okay to remove the
//   node and handle it itself
#define ENT_RES 1

  // directories have entries (linked list)
  struct DirectoryEntry {
    int type = ENT_RES;
    ck::string name;
    int nr = -1;  // for unresolved
    ck::ref<fs::Node> ino;

    // if this direntry is a mount, this inode will shadow it
    ck::ref<fs::Node> mount_shadow = nullptr;

    fs::DirectoryEntry *next, *prev;
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

// TTY file
#define T_TTY 8

  struct Ownership {
    int uid;
    int gid;
    int mode;  // the octal part of a file :)
  };



  struct FileOperations {
    // seek - used to notify of seeking. Doesn't actually seek
    //        returns -errno on failure (seek wasn't allowed)
    int (*seek)(fs::File &, off_t old_off, off_t new_off) = NULL;
    ssize_t (*read)(fs::File &, char *, size_t) = NULL;
    ssize_t (*write)(fs::File &, const char *, size_t) = NULL;
    int (*ioctl)(fs::File &, unsigned int, off_t) = NULL;

    /**
     * open - notify the driver that a new descriptor has opened the file
     * if it returns a non-zero code, it will be propagated back to the
     * sys::open() call and be considered a fail
     */
    int (*open)(fs::File &) = NULL;

    /**
     * close - notify the driver that a file descriptor is closing it
     *
     * no return value
     */
    void (*close)(fs::File &) = NULL;

    /* map a file into a vm area */
    ck::ref<mm::VMObject> (*mmap)(fs::File &, size_t npages, int prot, int flags, off_t off);
    // resize a file. if size is zero, it is a truncate
    int (*resize)(fs::File &, size_t);

    void (*destroy)(fs::Node &);

    // *quickly* poll for a bitmap of events and return a bitmap of those that match
    int (*poll)(fs::File &, int events, poll_table &pt);
  };

  // wrapper functions for block devices
  extern struct fs::FileOperations block_file_ops;

  struct DirectoryOperations {
    // create a file in the directory
    int (*create)(fs::Node &, const char *name, struct fs::Ownership &);
    // create a directory in a dir
    int (*mkdir)(fs::Node &, const char *, struct fs::Ownership &);
    // remove a file from a directory
    int (*unlink)(fs::Node &, const char *);
    // lookup an inode by name in a file
    ck::ref<fs::Node> (*lookup)(fs::Node &, const char *);
    // create a device node with a major and minor number
    int (*mknod)(fs::Node &, const char *name, struct fs::Ownership &, int major, int minor);
  };



  /**
   * struct inode - base point for all "file-like" objects
   *
   * RULE: if an inode has a pointer to another inode inside of it, it owns it.
   *       this means you cannot have circular references - hardlinks to one
   * file in two different dirs must have different `struct inode`s
   */
  struct Node : public ck::refcounted<Node> {
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

    fs::FileOperations *fops = NULL;
    fs::DirectoryOperations *dops = NULL;

    ck::ref<fs::SuperBlock> sb;

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
        ck::ref<fs::Node> mountpoint;
        struct DirectoryEntry *entries;
        // an "owned" string that represents the name
        const char *name;
      } dir;

      // T_BLK
      struct {
        fs::BlockDevice *dev;
      } blk;
    };


    // if this inode has a socket bound to it, it will be located here.
    net::Socket *sk = NULL;
    net::Socket *bound_socket = NULL;

    /*
     * the directory entry list in this->as.dir is a linked list that must be
     * traversed to find the entry. When the subclass creates a directory, it
     * must populate that list with at least the names of the entries. If an
     * entry contains a NULL inode, it calls 'resolve_direntry' which returns
     * the backing inode.
     */
    int register_direntry(ck::string name, int type, int nr, ck::ref<fs::Node> = nullptr);
    int remove_direntry(ck::string name);
    ck::ref<fs::Node> get_direntry(const char *name);
    int get_direntry_r(const char *name);
    void walk_direntries(ck::func<bool(const ck::string &, ck::ref<fs::Node>)>);
    ck::vec<ck::string> direntries(void);
    int add_mount(const char *name, ck::ref<fs::Node> other_root);

    struct DirectoryEntry *get_direntry_raw(const char *name);

    int poll(fs::File &, int events, poll_table &pt);

    // if the inode is a directory, set its name. NOP otherwise
    int set_name(const ck::string &);

    Node(int type, ck::ref<fs::SuperBlock> sb);
    virtual ~Node();

    int stat(struct stat *);
    spinlock lock;

   protected:
    int rc = 0;

   private:
    ck::ref<fs::Node> get_direntry_nolock(const char *name);
    ck::ref<fs::Node> get_direntry_ino(struct DirectoryEntry *);
  };


  // A `File` is an abstraction of all file-like objects
  // when they have been opened by a process. They are
  // basically just the backing structure referenced by
  // a filedescriptor from userspace.
  class File final : public ck::refcounted<File> {
    int m_error = 0;

   public:
    // must construct file descriptors via these factory funcs
    static ck::ref<File> create(ck::ref<fs::Node>, ck::string open_path, int flags = FDIR_READ | FDIR_WRITE);

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

    fs::FileOperations *fops(void);

    inline int errorcode() { return m_error; };

    ~File(void);


    File(ck::ref<fs::Node>, int flags);
    inline File() : File(nullptr, 0) {}

    inline operator bool() { return ino != nullptr; }

    ck::ref<fs::Node> ino;
    ck::string path;
    off_t m_offset = 0;

    bool can_write = false;
    bool can_read = false;

    int pflags = 0;  // private flags. Also used to track ptmx id
  };

};  // namespace fs


// fwd decl
namespace mm {
  struct Page;
};

/**
 * functions to interact with a block devices and the block cache
 */
namespace block {

  // a buffer represents a page (4k) in a block device.
  struct Buffer {
    fs::BlockDevice &bdev; /* the device this buffer belongs to */

    static struct Buffer *get(fs::BlockDevice &, off_t page);

    static void release(struct Buffer *);

    void register_write(void);

    ck::ref<mm::Page> page(void);

    // return the backing page data.
    void *data(void);
    off_t index(void);
    int flush(void);
    inline uint64_t last_used(void) { return m_last_used; }
    inline auto owners(void) {
      return m_count;  // XXX: race condition (!?!?)
    }

    size_t reclaim(void);

    inline bool dirty(void) { return m_dirty; }

   protected:
    inline static void release(struct blkdev *d) {}

    Buffer(fs::BlockDevice &, off_t);

    bool m_dirty = false;
    spinlock m_lock;
    int m_size = PGSIZE;
    uint64_t m_count = 0;
    off_t m_index;
    uint64_t m_last_used = 0;
    ck::ref<mm::Page> m_page;
  };


  size_t reclaim_memory(void);
  void sync_all(void);

};  // namespace block


// read data from blocks to from a byte offset. These can be somewhat wasteful,
// but that gets amortized by the block flush daemon :^)
int bread(fs::BlockDevice &, void *dst, size_t size, off_t byte_offset);
int bwrite(fs::BlockDevice &, void *data, size_t size, off_t byte_offset);

// reclaim some memory

inline auto bget(fs::BlockDevice &b, off_t page) { return block::Buffer::get(b, page); }

// release a block
static inline auto bput(struct block::Buffer *b) { return block::Buffer::release(b); }


struct bref {
  static inline bref get(fs::BlockDevice &b, off_t page) { return block::Buffer::get(b, page); }

  inline bref(struct block::Buffer *b) { buf = b; }

  inline ~bref(void) {
    if (buf) bput(buf);
  }

  inline struct block::Buffer *operator->(void) { return buf; }
  inline struct block::Buffer *get(void) { return buf; }


 private:
  struct block::Buffer *buf = NULL;
};

#endif
