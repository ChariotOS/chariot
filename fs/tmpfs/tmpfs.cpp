#include <fs/tmpfs.h>
#include <module.h>
#include <fs/vfs.h>
#include <errno.h>
#include "ck/ptr.h"
#include "fs.h"




int tmpfs::FileNode::seek_check(fs::File &, off_t old_off, off_t new_off) {
  if (new_off < 0 || new_off > size()) {
    return -EINVAL;
  }
  return 0;
}

ssize_t tmpfs::FileNode::read(fs::File &f, char *buf, size_t count) {
  scoped_lock l(m_lock);
  return access(f, (void *)buf, count, false);
}


ssize_t tmpfs::FileNode::write(fs::File &f, const char *buf, size_t count) {
  scoped_lock l(m_lock);
  auto end = f.offset() + count;
  // try to resize the file to have enough space
  if (int err = resize_r(f, end); err != 0) {
    return err;
  }
  return access(f, (void *)buf, count, true);
}


ssize_t tmpfs::FileNode::access(fs::File &f, void *dst, size_t size, bool write) {
	if (this->size() == 0) return 0;

  size_t byte_offset = f.offset();
  long to_access = size;
  // the offset within the current page
  ssize_t offset = byte_offset % PGSIZE;

  char *udata = (char *)dst;
	// write(13)
	// read(4096) -> read(13)
  if (this->size() <= byte_offset + size) {
		size = this->size() - byte_offset; // ??
	}

  for (off_t blk = byte_offset / PGSIZE; to_access > 0; blk++) {
		if (m_pages.size() < blk) break;
    // get the block we are looking at.
    auto page = m_pages[blk];
    if (!page) {
      break;
    }
    auto data = (char *)p2v(page->pa());

    size_t space_left = PGSIZE - offset;
    size_t can_access = min(space_left, to_access);

    if (write) {
      memcpy(data + offset, udata, can_access);
      page->fset(PG_DIRTY);
    } else {
      memcpy(udata, data + offset, can_access);
    }

    // moving on to the next block, we reset the offset
    offset = 0;
    to_access -= can_access;
    udata += can_access;

    if (to_access <= 0) break;
  }

  auto did_access = size - to_access;
  f.seek(did_access);

  return did_access;
}
int tmpfs::FileNode::resize(fs::File &file, size_t new_size) {
  scoped_lock l(m_lock);
  return resize_r(file, new_size);
}

int tmpfs::FileNode::resize_r(fs::File &file, size_t new_size) {
  size_t current_size = this->size();
  // early return if the size doesn't need to change
  if (current_size == new_size) return 0;

  auto old_page_count = round_up(current_size, 0x1000) >> 12;
  auto new_page_count = round_up(new_size, 0x1000) >> 12;

  if (new_page_count > old_page_count) {
    auto delta = new_page_count - old_page_count;
    ck::vec<ck::ref<mm::Page>> to_add;
    for (int i = 0; i < delta; i++) {
      auto page = mm::Page::alloc();
      if (!page) {
        return -ENOSPC;
      }
      to_add.push(page);
    }

    for (auto p : to_add) {
      m_pages.push(p);
    }
  }

  if (new_page_count < old_page_count) {
    m_pages.shrink(new_page_count);
  }
  set_size(new_size);
  return 0;
}



int tmpfs::DirNode::touch(ck::string name, fs::Ownership &own) {
  if (entries.contains(name)) return -EEXIST;
  auto node = ck::make_ref<tmpfs::FileNode>(sb);
  node->set_name(name);
  node->set_ownership(own);
  link(name, node);
  return 0;
}

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
