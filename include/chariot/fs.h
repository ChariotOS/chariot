#pragma once

#ifndef __FS__H__
#define __FS__H__

#include <errno.h>
#include <atom.h>
#include <dev/hardware.h>
#include <ck/func.h>
#include <lock.h>
#include <stat.h>
#include <ck/string.h>
#include <types.h>
#include <wait.h>


#define FS_REFACTOR() panic("fs refactor: Function called, but has not been rewritten.\n")


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


namespace devfs {
  class DirectoryNode;
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

#if 0
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
#endif



  struct FileSystem : public ck::refcounted<FileSystem> {
    //
    dev_t dev;
    long block_size;
    long max_filesize;
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

  extern ck::ref<fs::FileSystem> DUMMY_SB;



  struct SuperBlockOperations {
    // initialize the superblock after it has been mounted. All of the
    // arguments
    int (&init)(fs::FileSystem &);
    int (&write_super)(fs::FileSystem &);
    int (&sync)(fs::FileSystem &, int flags);
  };

  struct SuperBlockInfo {
    const char *name;

    // return a superblock containing the root inode
    ck::ref<fs::FileSystem> (&mount)(fs::SuperBlockInfo *, const char *args, int flags, const char *device);

    struct SuperBlockOperations &ops;
  };

// memory only
#define ENT_MEM 0
// resident on the backing storage
//   if the mode is this, the subclass will be asked if it is okay to remove the
//   node and handle it itself
#define ENT_RES 1

  // directories have entries (linked list)
  struct DirectoryEntry {
    DirectoryEntry(ck::string &name, ck::ref<fs::Node> ino) : name(name), ino(ino) {}
    ck::string name;
    ck::ref<fs::Node> ino;
    // if this direntry is a mount, this inode will shadow it
    ck::ref<fs::Node> mount_shadow = nullptr;

    ck::ref<fs::Node> get(void) const { return mount_shadow ? mount_shadow : ino; }
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

  /**
   * struct Node - base point for all "file-like" objects
   */
  struct Node : public ck::refcounted<Node> {
    ck::ref<fs::FileSystem> sb;
    // if this inode has a socket bound to it, it will be located here.
    ck::ref<net::Socket> sk;
    ck::ref<net::Socket> bound_socket;

    // if the inode is a directory, set its name. NOP otherwise

    Node(ck::ref<fs::FileSystem> sb);
    virtual ~Node();

    // Basic Node operations
    virtual int seek(fs::File &, off_t old_off, off_t new_off);
    virtual ssize_t read(fs::File &, char *dst, size_t count);
    virtual ssize_t write(fs::File &, const char *, size_t);
    virtual int ioctl(fs::File &, unsigned int, off_t);
    virtual int open(fs::File &);
    virtual void close(fs::File &);
    virtual ck::ref<mm::VMObject> mmap(fs::File &, size_t npages, int prot, int flags, off_t off);
    virtual int resize(fs::File &, size_t);
    virtual int stat(fs::File &, struct stat *);
    virtual int poll(fs::File &, int events, poll_table &pt);
    virtual ssize_t size(void);

    // Directory Operations
    // Create a FileNode in a directory
    virtual int touch(ck::string name, fs::Ownership &) { return -EINVAL; }
    // Create a DirectoryNode in a dir
    virtual int mkdir(ck::string name, fs::Ownership &) { return -EINVAL; }
    // Remove a file from a directory
    virtual int unlink(ck::string name) { return -EINVAL; }
    // Search a directory for a file
    virtual ck::ref<fs::Node> lookup(ck::string name) { return nullptr; }
    virtual int mknod(ck::string name, fs::Ownership &, int major, int minor) { return -EINVAL; }
    // Get a list of the directory entires in this directory.
    virtual ck::vec<DirectoryEntry *> dirents(void) { return {}; }
    // get a dirent (return null if it doesn't exist). It is assumed `lock` is held
    // while the parent holds the borrowed DirectoryEntry.
    virtual DirectoryEntry *get_direntry(ck::string name) { return nullptr; }
    // Link a Node as a DirectoryEntry (used for linking ".." and ".")
    virtual int link(ck::string name, ck::ref<fs::Node> node) { return -EINVAL; }


    virtual bool is_file(void) { return false; }
    virtual bool is_dir(void) { return false; }
    virtual bool is_sock(void) { return false; }
    virtual bool is_dev(void) { return false; }
    virtual bool is_blockdev(void) { return false; }
    virtual bool is_chardev(void) { return false; }

    scoped_lock lock(void) { return m_lock; }
    scoped_irqlock irq_lock(void) { return m_lock; }
    void set_name(const ck::string &s) { m_name = s; }
    const ck::string &name(void) const { return m_name; }

   protected:
    int rc = 0;
    spinlock m_lock;
    ck::string m_name;
  };


  class FileNode : public fs::Node {
   public:
    using fs::Node::Node;
    virtual bool is_file(void) final { return true; }
  };

  class DirectoryNode : public fs::Node {
   public:
    using fs::Node::Node;
    virtual bool is_dir(void) final { return true; }
    virtual ssize_t size(void) { return 0;} 
  };

  class SockNode : public fs::Node {
   public:
    using fs::Node::Node;
    virtual bool is_sock(void) final { return true; }
  };



  class DeviceNode : public fs::Node {
   public:
    DeviceNode();
    virtual ~DeviceNode();
    void bind(ck::string name);
    void unbind(void);
    const ck::string &get_name(void) const { return m_name; }


    virtual bool is_dev(void) final { return true; }

   protected:
    friend class devfs::DirectoryNode;

    static scoped_irqlock lock_names(void);
    static ck::map<ck::string, ck::box<fs::DirectoryEntry>> &get_names(void);

   private:
    ck::string m_name;
  };

	enum Direction {
		Read, Write
	};

  class BlockDeviceNode : public fs::DeviceNode {
   public:
    virtual bool is_blockdev(void) final { return true; }

		// ^fs::Node
    virtual ssize_t read(fs::File &, char *dst, size_t count) final;
    virtual ssize_t write(fs::File &, const char *, size_t) final;
    virtual ssize_t size(void) final;


    // nice wrappers for filesystems and all that :)
    inline int read_block(void *data, int block) { return read_blocks(block, data, 1); }
    inline int write_block(void *data, int block) { return write_blocks(block, data, 1); }

    // int rw_block(void *data, int block, fs::Direction dir);


    virtual ssize_t block_size(void) = 0;
    virtual ssize_t block_count(void) = 0;
    virtual int read_blocks(uint32_t sector, void* data, int n) = 0;
    virtual int write_blocks(uint32_t sector, const void* data, int n) = 0;
  };

  class CharDeviceNode : public fs::DeviceNode {
   public:
    virtual bool is_char(void) final { return true; }
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
    int stat(struct stat *stat) { return ino->stat(*this, stat); }
    int close();


    inline off_t offset(void) { return m_offset; }

    // fs::FileOperations *fops(void);

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
    fs::BlockDeviceNode &bdev; /* the device this buffer belongs to */

    static struct Buffer *get(fs::BlockDeviceNode &, off_t page);

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

    Buffer(fs::BlockDeviceNode &, off_t);

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
int bread(fs::BlockDeviceNode &, void *dst, size_t size, off_t byte_offset);
int bwrite(fs::BlockDeviceNode &, void *data, size_t size, off_t byte_offset);

// reclaim some memory

inline auto bget(fs::BlockDeviceNode &b, off_t page) { return block::Buffer::get(b, page); }

// release a block
static inline auto bput(struct block::Buffer *b) { return block::Buffer::release(b); }


struct bref {
  static inline bref get(fs::BlockDeviceNode &b, off_t page) { return block::Buffer::get(b, page); }

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
