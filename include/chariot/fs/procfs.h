#include <fs.h>



namespace procfs {

  // file and directory operations for
  extern fs::file_operations fops;
  extern fs::dir_operations dops;

  struct priv {};


  struct superblock : public fs::superblock {
    superblock(string args, int flags);

    /* create an inode and acquire it */
    fs::inode *create_inode(int type);

    spinlock lock;

   private:
    uint32_t next_inode = 0;
  };


  struct inode : public fs::inode {
		using fs::inode::inode;
    virtual ~inode();
  };

};  // namespace procfs
