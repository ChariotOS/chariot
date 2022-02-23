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

ssize_t tmpfs::FileNode::size(void) {
  printk("tmpfs size\n");
  return 0;
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



ck::ref<fs::FileSystem> tmpfs::FileSystem::mount(ck::string args, int flags, ck::string device) {
  return ck::make_ref<tmpfs::FileSystem>(args, flags);
}




void tmpfs::init(void) { vfs::register_filesystem<tmpfs::FileSystem>("tmpfs"); }

// module_init("fs::tmpfs", tmpfs_init);
