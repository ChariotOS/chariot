#include <fs.h>



namespace procfs {

  // file and directory operations for
  extern fs::FileOperations fops;
  extern fs::DirectoryOperations dops;

  struct priv {};


  struct SuperBlock : public fs::SuperBlock {
    SuperBlock(ck::string args, int flags);

    /* create an inode and acquire it */
    fs::Node *create_inode(int type);

    spinlock lock;

   private:
    uint32_t next_inode = 0;
  };


  struct inode : public fs::Node {
    using fs::Node::Node;
    virtual ~inode();
  };

};  // namespace procfs
