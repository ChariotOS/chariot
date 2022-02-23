#include <fs/tmpfs.h>
#include <module.h>
#include <fs/vfs.h>
#include <errno.h>
#include "ck/ptr.h"
#include "fs.h"




int tmpfs::FileNode::seek(fs::File &, off_t old_off, off_t new_off) { return -ENOTIMPL; }

ssize_t tmpfs::FileNode::read(fs::File &, char *buf, size_t count) { return -ENOTIMPL; }
ssize_t tmpfs::FileNode::write(fs::File &, const char *buf, size_t count) { return -ENOTIMPL; }

int tmpfs::FileNode::resize(fs::File &, size_t) { return -ENOTIMPL; }
int tmpfs::FileNode::stat(fs::File &, struct stat *s) {
  // TODO:
  return -ENOTIMPL;
}



int tmpfs::DirNode::touch(ck::string name, fs::Ownership &) { return -ENOTIMPL; }

int tmpfs::DirNode::mkdir(ck::string name, fs::Ownership &own) {
  auto node = ck::make_ref<tmpfs::DirNode>(sb);
	node->set_name(name);
  node->link(".", node);
  node->link("..", this);
	return link(name, node);
}

int tmpfs::DirNode::unlink(ck::string name) { return -ENOTIMPL; }

ck::ref<fs::Node> tmpfs::DirNode::lookup(ck::string name) { return nullptr; }

ck::vec<fs::DirectoryEntry *> tmpfs::DirNode::dirents(void) {
  ck::vec<fs::DirectoryEntry *> res;
  for (auto &[name, ent] : entries) {
    res.push(ent.get());
  }
  return res;
}

fs::DirectoryEntry *tmpfs::DirNode::get_direntry(ck::string name) {
	auto f = entries.find(name);
	if (f == entries.end()) return nullptr;
	return f->value.get();
}

int tmpfs::DirNode::link(ck::string name, ck::ref<fs::Node> node) {
  if (entries.contains(name)) return -EEXIST;
  auto ent = ck::make_box<fs::DirectoryEntry>(name, node);
  entries[name] = move(ent);
  return 0;
}


tmpfs::FileSystem::FileSystem(ck::string args, int flags) {
  /* by default, use 256 mb of pages */
  allowed_pages = 256 * MB / PGSIZE;
  used_pages = 0;

  /* Create the root INODE */
  root = ck::make_ref<tmpfs::DirNode>(this);
  root->link(".", root);
  root->link("..", root);
}


tmpfs::FileSystem::~FileSystem(void) { printk("tmpfs superblock dead\n"); }



static ck::ref<fs::FileSystem> tmpfs_mount(struct fs::SuperBlockInfo *, const char *args, int flags, const char *device) {
  return ck::make_ref<tmpfs::FileSystem>(args, flags);
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


void tmpfs::init(void) { vfs::register_filesystem(tmpfs_info); }

// module_init("fs::tmpfs", tmpfs_init);
