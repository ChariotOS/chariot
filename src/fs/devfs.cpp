#include <atom.h>
#include <dev/RTC.h>
#include <dev/device.h>
#include <dev/mbr.h>
#include <errno.h>
#include <fs/devfs.h>
#include <fs/vfs.h>
#include <map.h>
#include <module.h>
#include <vec.h>

#define DEVFS_ROOT_ID 42

// a list of all the devices on the system.
// TODO: turn this into a hashmap of the names
static map<string, unique_ptr<dev::device>> devices;

static auto initial_time = dev::RTC::now();

/**
 * the device filesytem is just a very simple wrapper around the static
 * functions defined in this file for accessing the devfs tree
 */

class devfs_wrapper : public fs::filesystem {
 public:
  devfs_wrapper() : fs::filesystem() {}

  virtual ~devfs_wrapper() {}
  virtual fs::vnoderef get_inode(u32 index);
  virtual fs::vnoderef get_root_inode();
  virtual u64 block_size(void) { return 0; }
  virtual bool init(void) { return true; };

  // the devfs cannot be unmounted
  virtual bool umount(void) { return false; }
};

static auto primary_devfs = new devfs_wrapper();

class devfs_inode : public fs::vnode {
 public:
  devfs_inode(u32 index, unique_ptr<dev::device> d = {})
      : fs::vnode(index), m_dev(move(d)) {}

  virtual fs::file_metadata metadata(void) {
    fs::file_metadata md;
    md.link_count = 1;

    // TODO: fill out the metadata correctly
    if (m_dev) {
      md.type = fs::file_type::file;
    } else {
      md.type = fs::file_type::dir;
    }

    return md;
  }

  virtual ssize_t read(fs::filedesc&, void*, size_t) { return -ENOTIMPL; }
  virtual ssize_t write(fs::filedesc&, void *, size_t) { return -ENOTIMPL; }

  virtual fs::filesystem& fs(void) { return *primary_devfs; }

  virtual bool walk_dir_impl(func<bool(const string&, u32)> cb) {
    for (auto& e : dir_entries) {
      if (!cb(e.name, e.index)) return false;
    }

    return true;
  }

  struct dir_entry {
    string name;
    u32 index;
    u32 mode;
  };

  /*
   * Cause a regular file to be truncated to the size of precisely length bytes.
   *
   * If the file previously was larger than this size, the extra data is lost.
   * If the file previously was shorter, it is extended, and the extended part
   * reads as null bytes ('\0').
   */
  virtual int truncate(size_t len) { return -ENOTIMPL; }

  virtual int add_dir_entry(ref<vnode> node, const string& name, u32 mode) {
    for (auto& e : dir_entries) {
      if (e.name == name) return -EEXIST;
    }

    dir_entries.push({.name = name, .index = node->index(), .mode = mode});
    return 0;
  }

  inline dev::device* get_dev() { return m_dev.get(); }

 private:
  // if this is null, then this inode is a directory;
  unique_ptr<dev::device> m_dev = {};

  vec<dir_entry> dir_entries;
  // map<string, u32> dir_entries;
};

static map<u32, ref<devfs_inode>> inode_ids;

int next_inode_index = DEVFS_ROOT_ID;
static auto allocate_devfs_inode(unique_ptr<dev::device> d = {}) {
  // TODO: lock ids
  int id = next_inode_index++;
  auto node = make_ref<devfs_inode>(id, move(d));
  inode_ids.set(id, node);

  if (!d) {
    node->add_dir_entry(node, ".", 0);
    if (id == DEVFS_ROOT_ID) node->add_dir_entry(node, "..", 0);
  }
  return node;
}

fs::vnoderef devfs_wrapper::get_inode(u32 index) {
  // TODO: lock!
  if (inode_ids.contains(index)) {
    auto n = inode_ids.get(index);
    return n;
  }

  // fails
  return {};
}

fs::vnoderef devfs_wrapper::get_root_inode() {
  return get_inode(DEVFS_ROOT_ID);
}

bool fs::devfs::init(void) {
  // allocate the root inode
  allocate_devfs_inode();

  return true;
}

// the dev filesystem gets mounted in a kernel module
bool fs::devfs::mount(void) {
  auto node = vfs::open("/dev", O_RDWR);
  if (!node) panic("unable to mount devfs to '/dev'. Unable to open '/dev'");
  int mounted = vfs::mount(unique_ptr(primary_devfs), node);
  return mounted == 0;
}

void fs::devfs::register_device(string name, unique_ptr<dev::device> dev,
                                u32 flags) {

  return;
  if (flags & DEVFS_REG_WALK_PARTS) {
    if (!dev->is_blk_device())
      panic("cannot walk partitions on a non-block device %s\n", name.get());

    // try MBR
    // TODO: add more partition schemes
    auto& bdev = *static_cast<dev::blk_dev*>(dev.get());

    dev::mbr parts(bdev);
    if (parts.parse()) {
      for_range(i, 0, parts.part_count()) {
        auto part = parts.partition(i);
        register_device(string::format("%sp%d", name.get(), i), move(part),
                        flags);
      }
    }
  }

  // TODO: lock
  auto n = allocate_devfs_inode(move(dev));

  printk("[DEVFS] registering %s to %d\n", name.get(), n->index());
  // add it to the root
  inode_ids[DEVFS_ROOT_ID]->add_dir_entry(n, name, 0);
}

dev::device* fs::devfs::get_device(string name) {
  dev::device* dev = nullptr;
  // TODO: LOCK THE DEVICE LIST
  inode_ids[DEVFS_ROOT_ID]->walk_dir(
      [&](const string& s, fs::vnoderef vn) -> bool {
        if (s == name) {
          auto g = static_cast<devfs_inode*>(vn.get());
          dev = g->get_dev();
          return false;
        }

        return true;
      });

  return dev;
}
