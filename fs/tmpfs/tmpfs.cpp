#include "tmpfs.h"
#include <module.h>
#include <fs/vfs.h>
#include <errno.h>
#include "ck/ptr.h"
#include "fs.h"




int tmp::FileNode::seek(fs::File &, off_t old_off, off_t new_off) { return -ENOTIMPL; }

ssize_t tmp::FileNode::read(fs::File &, char *buf, size_t count) { return -ENOTIMPL; }
ssize_t tmp::FileNode::write(fs::File &, const char *buf, size_t count) { return -ENOTIMPL; }

int tmp::FileNode::resize(fs::File &, size_t) { return -ENOTIMPL; }
int tmp::FileNode::stat(fs::File &, struct stat *s) {
  // TODO:
  return -ENOTIMPL;
}



int tmp::DirNode::touch(ck::string name, fs::Ownership &) { return -ENOTIMPL; }

int tmp::DirNode::mkdir(ck::string name, fs::Ownership &own) {
  auto node = ck::make_ref<tmp::DirNode>(sb);
	node->set_name(name);
  node->link(".", node);
  node->link("..", this);
	return link(name, node);
}

int tmp::DirNode::unlink(ck::string name) { return -ENOTIMPL; }

ck::ref<fs::Node> tmp::DirNode::lookup(ck::string name) { return nullptr; }

ck::vec<fs::DirectoryEntry *> tmp::DirNode::dirents(void) {
  ck::vec<fs::DirectoryEntry *> res;
  for (auto &[name, ent] : entries) {
    res.push(ent.get());
  }
  return res;
}

fs::DirectoryEntry *tmp::DirNode::get_direntry(ck::string name) {
	auto f = entries.find(name);
	if (f == entries.end()) return nullptr;
	return f->value.get();
}

int tmp::DirNode::link(ck::string name, ck::ref<fs::Node> node) {
  if (entries.contains(name)) return -EEXIST;
  auto ent = ck::make_box<fs::DirectoryEntry>(name, node);
  entries[name] = move(ent);
  return 0;
}


tmp::FileSystem::FileSystem(ck::string args, int flags) {
  /* by default, use 256 mb of pages */
  allowed_pages = 256 * MB / PGSIZE;
  used_pages = 0;

  /* Create the root INODE */
  root = ck::make_ref<tmp::DirNode>(this);
  root->link(".", root);
  root->link("..", root);
}


tmp::FileSystem::~FileSystem(void) { printk("tmpfs superblock dead\n"); }



static ck::ref<fs::FileSystem> tmpfs_mount(struct fs::SuperBlockInfo *, const char *args, int flags, const char *device) {
  return ck::make_ref<tmp::FileSystem>(args, flags);
}


int tmpfs_sb_init(struct fs::FileSystem &sb) { return -ENOTIMPL; }

int tmpfs_write_super(struct fs::FileSystem &sb) { return -ENOTIMPL; }

int tmpfs_sync(struct fs::FileSystem &sb, int flags) { return -ENOTIMPL; }


struct fs::SuperBlockOperations tmpfs_ops {
  .init = tmpfs_sb_init, .write_super = tmpfs_write_super, .sync = tmpfs_sync,
};

struct fs::SuperBlockInfo tmpfs_info {
  .name = "tmpfs", .mount = tmpfs_mount, .ops = tmpfs_ops,
};


static void tmpfs_init(void) { vfs::register_filesystem(tmpfs_info); }
module_init("fs::tmpfs", tmpfs_init);
