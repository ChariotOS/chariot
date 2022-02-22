#pragma once


#include <fs.h>
#include <mm.h>
#include <ck/vec.h>

namespace tmp {

  // private data per inode
  struct priv {
    /* The data pages */
    ck::vec<ck::ref<mm::Page>> pages;
  };




  extern fs::FileOperations fops;
  extern fs::DirectoryOperations dops;


  struct FileSystem : public fs::FileSystem {
    /* memory statistics, to keep tmpfs from using too much memory */
    size_t allowed_pages;
    size_t used_pages;
    spinlock lock;
    uint64_t next_inode = 0;


    FileSystem(ck::string args, int flags);

    virtual ~FileSystem();

    /* create an inode and acquire it */
    ck::ref<fs::Node> create_inode(int type);
  };


  inline auto &getsb(fs::Node &i) { return *(tmp::FileSystem *)(i.sb.get()); }


  /*
   * Directory Operations
   */
  namespace dop {}


}  // namespace tmp
