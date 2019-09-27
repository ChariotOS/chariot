#include <fs/tmpfs.h>
#include <fs/vnode.h>

enum class tmp_type : u8 { file, dir, symlink, blockdev, chardev };

class tmp_vnode : public fs::vnode {
 public:
  tmp_vnode(u32 index, fs::tmp& the_fs, fs::file_type type, u32 mode)
      : fs::vnode(index), m_fs(the_fs), type(type), m_mode(mode) {}

  virtual fs::file_metadata metadata(void) {
    fs::file_metadata md;
    md.link_count = link_count;

    md.type = type;

    return md;
  }

  virtual ssize_t read(fs::filedesc&, void*, size_t) { return -ENOTIMPL; }
  virtual ssize_t write(fs::filedesc&, void*, size_t) { return -ENOTIMPL; }

  virtual fs::filesystem& fs(void) { return m_fs; }

  virtual bool walk_dir_impl(func<bool(const string&, u32)> cb) {
    if (!is_dir()) return false;

    for (auto& e : dir_entries) {
      if (!cb(e.name, e.index)) return false;
    }
    return true;
  }

  struct dir_entry {
    string name;
    u32 index;
  };

  /*
   * Cause a regular file to be truncated to the size of precisely length bytes.
   *
   * If the file previously was larger than this size, the extra data is lost.
   * If the file previously was shorter, it is extended, and the extended part
   * reads as null bytes ('\0').
   */
  virtual int truncate(size_t len) { return -ENOTIMPL; }

  virtual int add_dir_entry(ref<vnode> node, const string& name,
                            u32 mode = 0 /* ignored ? */) {
    for (auto& e : dir_entries) {
      if (e.name == name) return -EEXIST;
    }

    dir_entries.push({.name = name, .index = node->index()});
    return 0;
  }

  virtual ref<vnode> mkdir(string name, u32 mode = 0000) {
    // fail if not a dir
    if (!is_dir()) return nullptr;

    // TODO: add a directory, then add this dir as .. and itself as .
    auto node = m_fs.alloc_node(fs::file_type::dir, mode);

    // add the directory defaults
    node->add_dir_entry(node, ".", mode);
    node->add_dir_entry(this, "..", m_mode);

    add_dir_entry(node, name, mode);

    return node;
  }
  virtual ref<vnode> touch(string name, fs::file_type type, u32 mode = 0000) {
    // fail if not a dir
    if (!is_dir()) return nullptr;

    // allocate a node of a certain type
    auto node = m_fs.alloc_node(type, mode);
    add_dir_entry(node, name, mode);
    // add a new file to the dir entries
    return node;
  }

 private:
  // if this is null, then this inode is a directory;
  fs::tmp& m_fs;
  fs::file_type type;
  u32 m_mode = 0000;
  vec<dir_entry> dir_entries;
  int link_count = 1;
};

class tmpinode : public fs::vnode {};

fs::tmp::tmp(void) {
  // create the root
  m_root = alloc_node(fs::file_type::dir, 0777);
  m_root->add_dir_entry(m_root, ".", 0777);
  m_root->add_dir_entry(m_root, "..", 0777);
}

fs::tmp::~tmp(void) {
  // TODO: clear all the references
}

u64 fs::tmp::block_size() { return 512; }

bool fs::tmp::init(void) {
  // TODO: allocate the first inode for root
  return true;
}

fs::vnoderef fs::tmp::get_inode(u32 index) {
  if (inodemap.contains(index)) {
    return inodemap[index];
  }

  return nullptr;
}

fs::vnoderef fs::tmp::get_root_inode(void) { return m_root; }

bool fs::tmp::umount(void) {
  // check if any of the inodes are referenced somewhere else
  return false;
}

fs::vnoderef fs::tmp::alloc_node(fs::file_type type, u32 mode) {
  // TODO: take a lock
  auto node = make_ref<tmp_vnode>(m_next_inode, *this, type, mode);
  inodemap[m_next_inode++] = node;
  return node;
}

