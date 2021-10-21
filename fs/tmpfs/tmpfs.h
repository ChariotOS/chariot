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




  extern fs::file_operations fops;
  extern fs::dir_operations dops;


  struct superblock : public fs::superblock {
    /* memory statistics, to keep tmpfs from using too much memory */
    size_t allowed_pages;
    size_t used_pages;
    spinlock lock;
    uint64_t next_inode = 0;


    superblock(ck::string args, int flags);


    /* create an inode and acquire it */
    fs::inode *create_inode(int type);
  };


  inline auto &getsb(fs::inode &i) { return *(tmp::superblock *)&i.sb; }


  /*
   * Directory Operations
   */
  namespace dop {}


}  // namespace tmp
