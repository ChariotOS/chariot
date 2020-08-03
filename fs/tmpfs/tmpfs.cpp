#include "tmpfs.h"
#include <module.h>
#include <fs/vfs.h>
#include <errno.h>

static struct fs::superblock *tmpfs_mount(struct fs::sb_information *,
                                         const char *args, int flags,
                                         const char *device) {
	/*
  struct fs::blkdev *bdev = fs::bdev_from_path(device);
  if (bdev == NULL) return NULL;

  auto *sb = new fs::tmpfs();

  if (!sb->init(bdev)) {
    delete sb;
    return NULL;
  }

  return sb;
	*/

	return NULL;
}


int tmpfs_sb_init(struct fs::superblock &sb) { return -ENOTIMPL; }

int tmpfs_write_super(struct fs::superblock &sb) { return -ENOTIMPL; }

int tmpfs_sync(struct fs::superblock &sb, int flags) { return -ENOTIMPL; }


struct fs::sb_operations tmpfs_ops {
  .init = tmpfs_sb_init, .write_super = tmpfs_write_super, .sync = tmpfs_sync,
};

struct fs::sb_information tmpfs_info {
  .name = "tmpfs", .mount = tmpfs_mount, .ops = tmpfs_ops,
};


static void tmpfs_init(void) { vfs::register_filesystem(tmpfs_info); }
module_init("fs::tmpfs", tmpfs_init);
