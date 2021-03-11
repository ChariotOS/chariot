#include <fs/procfs.h>
#include <errno.h>


fs::file_operations procfs::fops;


procfs::inode::~inode(void) {
	printk("procfs inode dtor\n");
}


// create a file in the directory
static int procfs_create(fs::inode &ino, const char *name, struct fs::file_ownership &own) {
	UNIMPL();
  return -ENOTIMPL;
}

// create a directory in a dir
static int procfs_mkdir(fs::inode &ino, const char *name, struct fs::file_ownership &own) {
	UNIMPL();
  return -ENOTIMPL;
}
// remove a file from a directory
static int procfs_unlink(fs::inode &node, const char *entry) {
	UNIMPL();
  return -ENOTIMPL;
}

// lookup an inode by name in a file
static struct fs::inode *procfs_lookup(fs::inode &node, const char *needle) {
  if (node.type != T_DIR) panic("procfs_lookup on non-dir\n");

  // walk the linked list to get the inode num
  for (auto *it = node.dir.entries; it != NULL; it = it->next) {
    if (!strcmp(it->name.get(), needle)) {
      return it->ino;
    }
  }
  return NULL;
}
// create a device node with a major and minor number
static int procfs_mknod(fs::inode &, const char *name, struct fs::file_ownership &, int major,
                       int minor) {
  UNIMPL();
  return -ENOTIMPL;
}



fs::dir_operations procfs::dops = {
    .create = procfs_create,
    .mkdir = procfs_mkdir,
    .unlink = procfs_unlink,
    .lookup = procfs_lookup,
    .mknod = procfs_mknod,
};
