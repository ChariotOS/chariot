#include <fs/procfs.h>
#include <lock.h>
#include <module.h>
#include <fs/vfs.h>
#include <errno.h>

procfs::SuperBlock::SuperBlock(ck::string args, int flags) {
  /* TODO */

  /* Create the root INODE */
  root = create_inode(T_DIR);
  root->register_direntry(".", ENT_MEM, root->ino, root);
  root->register_direntry("..", ENT_MEM, root->ino, root);
}


ck::ref<fs::Node> procfs::SuperBlock::create_inode(int type) {
  scoped_lock l(lock);
  auto ino = ck::make_ref<fs::Node>(type, this);
  ino->ino = next_inode++;
  ino->uid = 0;
  ino->gid = 0;
  ino->size = 0;  // zero size by default
  ino->mode = 07755;
  ino->link_count = 3;
  /* link up the file operations and directory operations */
  ino->dops = &procfs::dops;
  ino->fops = &procfs::fops;

  // allocate the private data
  ino->priv<procfs::priv>() = new procfs::priv();
  return ino;
}


static ck::ref<fs::SuperBlock> procfs_mount(struct fs::SuperBlockInfo *, const char *args, int flags, const char *device) {
  auto sb = ck::make_ref<procfs::SuperBlock>(args, flags);

  return sb;
}


int procfs_sb_init(struct fs::SuperBlock &sb) { return -ENOTIMPL; }

int procfs_write_super(struct fs::SuperBlock &sb) { return -ENOTIMPL; }

int procfs_sync(struct fs::SuperBlock &sb, int flags) { return -ENOTIMPL; }


struct fs::SuperBlockOperations procfs_ops {
  .init = procfs_sb_init, .write_super = procfs_write_super, .sync = procfs_sync,
};

struct fs::SuperBlockInfo procfs_info {
  .name = "procfs", .mount = procfs_mount, .ops = procfs_ops,
};


static void procfs_init(void) { vfs::register_filesystem(procfs_info); }
module_init("fs::procfs", procfs_init);
