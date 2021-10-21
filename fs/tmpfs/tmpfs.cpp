#include "tmpfs.h"
#include <module.h>
#include <fs/vfs.h>
#include <errno.h>

static ck::ref<fs::SuperBlock> tmpfs_mount(struct fs::SuperBlockInfo *, const char *args, int flags, const char *device) {
  return ck::make_ref<tmp::SuperBlock>(args, flags);
}


int tmpfs_sb_init(struct fs::SuperBlock &sb) { return -ENOTIMPL; }

int tmpfs_write_super(struct fs::SuperBlock &sb) { return -ENOTIMPL; }

int tmpfs_sync(struct fs::SuperBlock &sb, int flags) { return -ENOTIMPL; }


struct fs::SuperBlockOperations tmpfs_ops {
  .init = tmpfs_sb_init, .write_super = tmpfs_write_super, .sync = tmpfs_sync,
};

struct fs::SuperBlockInfo tmpfs_info {
  .name = "tmpfs", .mount = tmpfs_mount, .ops = tmpfs_ops,
};


static void tmpfs_init(void) { vfs::register_filesystem(tmpfs_info); }
module_init("fs::tmpfs", tmpfs_init);
