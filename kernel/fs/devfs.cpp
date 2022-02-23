#include <errno.h>
#include <fs.h>
#include <fs/devfs.h>
#include <fs/vfs.h>
#include <module.h>
#include <dev/driver.h>
#include <fs/devfs.h>




int devfs::DirectoryNode::touch(ck::string name, fs::Ownership &) { return -EINVAL; }
int devfs::DirectoryNode::mkdir(ck::string name, fs::Ownership &own) { return -EINVAL; }

int devfs::DirectoryNode::unlink(ck::string name) { return -ENOTIMPL; }
ck::ref<fs::Node> devfs::DirectoryNode::lookup(ck::string name) { return nullptr; }

ck::vec<fs::DirectoryEntry *> devfs::DirectoryNode::dirents(void) {
  ck::vec<fs::DirectoryEntry *> res;
  for (auto &[name, ent] : entries)
    res.push(ent.get());

  auto l = fs::DeviceNode::lock_names();
  const auto &names = fs::DeviceNode::get_names();
  for (auto &[name, ent] : names)
    res.push(ent.get());
  return res;
}

fs::DirectoryEntry *devfs::DirectoryNode::get_direntry(ck::string name) {
  auto f = entries.find(name);
  if (f != entries.end()) {
    return f->value.get();
  }


  auto l = fs::DeviceNode::lock_names();
  const auto &names = fs::DeviceNode::get_names();
  auto df = names.find(name);
  if (df != names.end()) return df->value.get();

  // TODO: search named device nodes
  return nullptr;
}

int devfs::DirectoryNode::link(ck::string name, ck::ref<fs::Node> node) {
  if (entries.contains(name)) return -EEXIST;
  auto ent = ck::make_box<fs::DirectoryEntry>(name, node);
  entries[name] = move(ent);
  return 0;
}

devfs::FileSystem::FileSystem(ck::string args, int flags) {
  root = ck::make_ref<devfs::DirectoryNode>(this);
  root->link(".", root);
  root->link("..", root);
}


devfs::FileSystem::~FileSystem(void) {}

static ck::ref<fs::FileSystem> devfs_mount(struct fs::SuperBlockInfo *, const char *args, int flags, const char *device) {
  return ck::make_ref<devfs::FileSystem>(args, flags);
}


int devfs_sb_init(struct fs::FileSystem &sb) { return -ENOTIMPL; }

int devfs_write_super(struct fs::FileSystem &sb) { return -ENOTIMPL; }

int devfs_sync(struct fs::FileSystem &sb, int flags) { return -ENOTIMPL; }


struct fs::SuperBlockOperations devfs_ops {
  .init = devfs_sb_init, .write_super = devfs_write_super, .sync = devfs_sync,
};

struct fs::SuperBlockInfo devfs_info {
  .name = "devfs", .mount = devfs_mount, .ops = devfs_ops,
};


void devfs::init(void) { vfs::register_filesystem(devfs_info); }
