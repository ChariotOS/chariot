#include <fs.h>



namespace procfs {

  // file and directory operations for
  extern fs::FileOperations fops;
  extern fs::DirectoryOperations dops;

  struct priv {};


  struct FileSystem : public fs::FileSystem {
    FileSystem(ck::string args, int flags);

    /* create an inode and acquire it */
    ck::ref<fs::Node> create_inode(int type);

    spinlock lock;

   private:
    uint32_t next_inode = 0;
  };



};  // namespace procfs
