#include <errno.h>
#include <fs.h>
#include <fs/devfs.h>
#include <fs/vfs.h>
#include <module.h>
#include <dev/driver.h>

static ck::ref<fs::Node> devfs_lookup(fs::Node &node, const char *needle) {
  if (node.type != T_DIR) panic("devfs_lookup on non-dir\n");

  // walk the linked list to get the inode num
  for (auto *it = node.dir.entries; it != NULL; it = it->next) {
    if (!strcmp(it->name.get(), needle)) {
      return it->ino;
    }
  }
  return nullptr;
}

static int devfs_create(fs::Node &node, const char *name, struct fs::Ownership &) { return -EPERM; }

static int devfs_mkdir(fs::Node &, const char *name, struct fs::Ownership &) { return -EPERM; }

static int devfs_unlink(fs::Node &, const char *) { return -EPERM; }

static int devfs_mknod(fs::Node &, const char *name, struct fs::Ownership &, int major, int minor) { return -EPERM; }

fs::DirectoryOperations devfs_dir_ops{
    .create = devfs_create,
    .mkdir = devfs_mkdir,
    .unlink = devfs_unlink,
    .lookup = devfs_lookup,
    .mknod = devfs_mknod,
};

int devfs_sb_init(struct fs::FileSystem &sb) {
  // construct the root node
  return -1;
}

int devfs_write_super(struct fs::FileSystem &sb) {
  // doesn't make sense
  return 0;
}
int devfs_sync(struct fs::FileSystem &sb, int flags) {
  // no need to sync
  return 0;
}


static auto devfs_sb = ck::make_ref<fs::FileSystem>();
static ck::ref<fs::Node> devfs_root = nullptr;
static int next_inode_nr = 0;
static auto devfs_create_inode(int type) {
  auto ino = ck::make_ref<fs::Node>(type, devfs_sb);
  ino->ino = next_inode_nr++;
  ino->uid = 0;
  ino->gid = 0;
  ino->size = 512;  // idk, whatever
  ino->mode = 07755;
  ino->link_count = 3;
  ino->dops = &devfs_dir_ops;
  return ino;
}

static ck::ref<fs::Node> devfs_get_root(void) {
  if (devfs_root == nullptr) {
    devfs_root = devfs_create_inode(T_DIR);
    devfs_root->register_direntry(".", ENT_MEM, devfs_root->ino, devfs_root);
    devfs_root->register_direntry("..", ENT_MEM, devfs_root->ino, devfs_root);
  }
  return devfs_root;
}

void devfs_register_device(ck::string name, int type, int major, int minor) {
  auto root = devfs_get_root();

  auto node = devfs_create_inode(type);
  node->major = major;
  node->minor = minor;
  dev::populate_inode_device(*node);
  root->register_direntry(name, ENT_MEM, node->ino, node);
}

struct fs::SuperBlockOperations devfs_sb_ops {
  .init = devfs_sb_init, .write_super = devfs_write_super, .sync = devfs_sync,
};


static ck::ref<fs::FileSystem> devfs_mount(struct fs::SuperBlockInfo *, const char *args, int flags, const char *device) {
  devfs_sb->ops = &devfs_sb_ops;
  devfs_sb->arguments = args;
  devfs_sb->root = devfs_get_root();
  return devfs_sb;
}


struct fs::SuperBlockInfo devfs_info {
  .name = "devfs", .mount = devfs_mount, .ops = devfs_sb_ops,
};

static void devfs_init(void) { vfs::register_filesystem(devfs_info); }

module_init("fs::devfs", devfs_init);
