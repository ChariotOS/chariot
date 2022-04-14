#pragma once

#include <types.h>
#include <ptr.h>
#include <fs/poll_table.h>
#include <ck/string.h>
#include <errno.h>
#include <lock.h>
#include <ck/map.h>

#define T_INVA 0
#define T_DIR 1
#define T_FILE 2
#define T_FIFO 3
#define T_CHAR 4
#define T_BLK 5
#define T_SYML 6
#define T_SOCK 7
#define T_TTY 8


#define FDIR_READ 1
#define FDIR_WRITE 2

#define SEEK_SET (-1)
#define SEEK_CUR (-2)
#define SEEK_END (-3)


// Gross forward declarations
namespace mm {
  struct VMObject;
  struct Page;
}  // namespace mm
namespace net {
  class Socket;
}
namespace devfs {
  class DirectoryNode;
}

namespace fs {
  // More forward declarations
  class Node;
  class File;
  class FileSystem;

  // A DirectoryNode has many DirectoryEntries, which give fs::Nodes names.
  struct DirectoryEntry {
    DirectoryEntry(ck::string &name, ck::ref<fs::Node> ino) : name(name), ino(ino) {}
    ck::string name;
    ck::ref<fs::Node> ino;
    // if this direntry is a mount, this inode will shadow it
    ck::ref<fs::Node> mount_shadow = nullptr;
    ck::ref<fs::Node> get(void) const { return mount_shadow ? mount_shadow : ino; }
  };

  struct Ownership {
    int uid = 0;      // user: root
    int gid = 0;      // group: root
    int mode = 0755;  // default access mode
  };




  // MetaData for a fs::Node to maintain
  struct MetaData {
    uint32_t inode = 0;
    uint32_t nlink = 0;
    // how many bytes there are in the file
    uint64_t size = 0;
    // How many blocks there are in this file
    uint32_t block_count = 0;
    // How big each block is
    uint32_t block_size = 0;
    // Various timing information (currently unused)
    time_t accessed_time = 0;
    time_t modified_time = 0;
    time_t create_time = 0;
  };


  /**
   * struct Node - base point for all "file-like" objects
   */
  class Node : public ck::refcounted<Node> {
   public:
    ck::ref<fs::FileSystem> sb;

    // If a filesystem node has had a socket bound to it, that socket will
    // be placed here.
    ck::ref<net::Socket> bound_socket;

    Node(ck::ref<fs::FileSystem> sb);
    virtual ~Node();

    //
    // File operations
    //
    // Read some bytes
    virtual ssize_t read(fs::File &file, char *buf, size_t sz);
    // Write some bytes
    virtual ssize_t write(fs::File &file, const char *buf, size_t sz);
    // Implement ioctl functionality that is specific to the Node
    virtual int ioctl(fs::File &file, unsigned int cmd, off_t arg);
    // Notify the fs::Node implementation that it has been opened into a fs::File instance
    virtual int open(fs::File &file);
    // Notify the fs::Node implementation that it has been closed by a fs::File instance
    virtual void close(fs::File &file);
    // Request a VMObject from the fs::Node implementation for mapping into virtual memory
    virtual ck::ref<mm::VMObject> mmap(fs::File &file, size_t npages, int prot, int flags, off_t off);
    // Resize the node
    virtual int resize(fs::File &file, size_t new_size);
    // Query the fs::Node implementation as to which a seek is valid
    virtual int seek_check(fs::File &file, off_t old_off, off_t new_off);
    // Populate a poll_table for usage with the `poll` systemcall
    virtual int poll(fs::File &file, int events, poll_table &pt);


    //
    // Directory Operations
    //
    // Create a FileNode in a directory
    virtual int touch(ck::string name, fs::Ownership &) { return -EINVAL; }
    // Create a DirectoryNode in a dir
    virtual int mkdir(ck::string name, fs::Ownership &) { return -EINVAL; }
    // Remove a file from a directory
    virtual int unlink(ck::string name) { return -EINVAL; }
    // Search a directory for a file
    ck::ref<fs::Node> lookup(ck::string name);
    virtual int mknod(ck::string name, fs::Ownership &, int major, int minor) { return -EINVAL; }
    // Get a list of the directory entires in this directory.
    virtual ck::vec<DirectoryEntry *> dirents(void) { return {}; }
    // get a dirent (return null if it doesn't exist). It is assumed `lock` is held
    // while the parent holds the borrowed DirectoryEntry.
    virtual DirectoryEntry *get_direntry(ck::string name) { return nullptr; }
    // Link a Node as a DirectoryEntry (used for linking ".." and ".")
    virtual int link(ck::string name, ck::ref<fs::Node> node) { return -EINVAL; }


    // Various query functions to determine the type (mode) of the fs::Node implementation
    virtual bool is_file(void) { return false; }      // fs::FileNode
    virtual bool is_dir(void) { return false; }       // fs::DirectoryNode
    virtual bool is_sock(void) { return false; }      // net::Socket
    virtual bool is_dev(void) { return false; }       // dev::Device
    virtual bool is_blockdev(void) { return false; }  // dev::BlockDevice
    virtual bool is_chardev(void) { return false; }   // dev::CharDevice
    virtual bool is_tty(void) { return false; }       // TTYNode

    // Lock the fs::Node and return a scoped_*lock to ensure release at some point
    scoped_lock lock(void) { return m_lock; }
    scoped_irqlock irq_lock(void) { return m_lock; }


    // Getters and setters for various fields. These are basically just
    getset(name, m_name);
    getset(ownership, m_ownership);
    getset(metadata, m_metadata);

    // Methods to access MetaData
    getset(size, m_metadata.size);
    getset(inode, m_metadata.inode);
    getset(nlink, m_metadata.nlink);
    getset(block_size, m_metadata.block_size);
    getset(block_count, m_metadata.block_count);
    getset(accessed_time, m_metadata.accessed_time);
    getset(create_time, m_metadata.create_time);
    getset(modified_time, m_metadata.modified_time);

    // Methods to access Ownership
    getset(gid, m_ownership.gid);
    getset(uid, m_ownership.uid);
    getset(mode, m_ownership.mode);

    int stat(struct stat *);

   protected:
    fs::Ownership m_ownership;
    fs::MetaData m_metadata;
    int rc = 0;
    spinlock m_lock;
    ck::string m_name;
  };


  /*
   * fs::FileNode
   *
   * A filesystem node that looks like a regular file.
   */
  class FileNode : public fs::Node {
   public:
    using fs::Node::Node;
    // ^fs::Node
    bool is_file(void) override final { return true; }
  };

  /*
   * fs::DirectoryNode
   *
   * A filesystem node that looks like a directory which contains other Nodes
   */
  class DirectoryNode : public fs::Node {
   public:
    using fs::Node::Node;
    // ^fs::Node
    bool is_dir(void) override final { return true; }
  };

}  // namespace fs
