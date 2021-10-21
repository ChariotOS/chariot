#include <fs/procfs.h>
#include <errno.h>


fs::FileOperations procfs::fops;


// procfs::inode::~inode(void) { printk("procfs inode dtor\n"); }


// create a file in the directory
static int procfs_create(fs::Node &ino, const char *name, struct fs::Ownership &own) {
  UNIMPL();
  return -ENOTIMPL;
}

// create a directory in a dir
static int procfs_mkdir(fs::Node &ino, const char *name, struct fs::Ownership &own) {
  UNIMPL();
  return -ENOTIMPL;
}
// remove a file from a directory
static int procfs_unlink(fs::Node &node, const char *entry) {
  UNIMPL();
  return -ENOTIMPL;
}

// lookup an inode by name in a file
static ck::ref<fs::Node> procfs_lookup(fs::Node &node, const char *needle) {
  if (node.type != T_DIR) panic("procfs_lookup on non-dir\n");

  // walk the linked list to get the inode num
  for (auto *it = node.dir.entries; it != NULL; it = it->next) {
    if (!strcmp(it->name.get(), needle)) {
      return it->ino;
    }
  }
  return nullptr;
}
// create a device node with a major and minor number
static int procfs_mknod(fs::Node &, const char *name, struct fs::Ownership &, int major, int minor) {
  UNIMPL();
  return -ENOTIMPL;
}



fs::DirectoryOperations procfs::dops = {
    .create = procfs_create,
    .mkdir = procfs_mkdir,
    .unlink = procfs_unlink,
    .lookup = procfs_lookup,
    .mknod = procfs_mknod,
};
