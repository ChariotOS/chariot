#include "tmpfs.h"



tmp::FileSystem::FileSystem(ck::string args, int flags) {
  /* by default, use 256 mb of pages */
  allowed_pages = 256 * MB / PGSIZE;
  used_pages = 0;

  /* Create the root INODE */
  root = create_inode(T_DIR);
  root->register_direntry(".", ENT_MEM, root->ino, root);
  root->register_direntry("..", ENT_MEM, root->ino, root);
}


tmp::FileSystem::~FileSystem(void) { printk("tmpfs superblock dead\n"); }


ck::ref<fs::Node> tmp::FileSystem::create_inode(int type) {
  scoped_lock l(lock);
  auto ino = ck::make_ref<fs::Node>(type, this);
  ino->ino = next_inode++;
  ino->uid = 0;
  ino->gid = 0;
  ino->size = 0;  // zero size by default
  ino->mode = 07755;
  ino->link_count = 3;
  /* link up the file operations and directory operations */
  ino->dops = &tmp::dops;
  ino->fops = &tmp::fops;

  // allocate the private data
  ino->priv<tmp::priv>() = new tmp::priv();
  return ino;
  // return ino;
}
