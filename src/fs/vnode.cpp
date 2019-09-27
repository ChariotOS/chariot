#include <fs/filesystem.h>
#include <fs/vfs.h>
#include <fs/vnode.h>

fs::vnode::vnode(u32 index) : m_index(index) {}
fs::vnode::~vnode(void) {}

fs::inode::inode(u32 fsid, u32 index) : m_fsid(fsid), m_index(index) {}

// so we dont construct these all the time in walk_dir
static string dot = ".";
static string dotdot = "..";

bool fs::vnode::walk_dir(func<bool(const string &, ref<vnode>)> cb) {
  // fail immediately if it wasnt a directory
  if (!is_dir()) {
    return false;
  }

  // Because we are a directory, we need to check if we are a mount point
  bool is_mounted_root = false;

  fs::vnoderef host_parent = {};

  auto qual = vfs::qualified_inode_number(fs(), index());
  auto host = vfs::get_mount_host(qual);

  if (host.get() != nullptr) {
    is_mounted_root = true;

    host->walk_dir([&](const string &s, vnoderef parent) {
      if (s == dotdot) {
        host_parent = parent;
        return false;
      }
      return true;
    });
  }

  // call the inode implementation of walk_dir, but stand between and handle
  // mount points
  return walk_dir_impl([&](const string &s, u32 in) -> bool {
    auto qual = vfs::qualified_inode_number(fs(), in);

    auto vn = vfs::get_mount_at(qual);
    if (is_mounted_root && s == dotdot && host_parent) {
      return cb(s, host_parent);
    }
    // printk("%s: %p\n", s.get(), vn.get());
    // TODO: handle if vn->index is a mount point
    if (!vn) vn = fs().get_inode(in);
    return cb(s, vn);
  });
}

// get the filesystem information from an actual filesystem
fs::inode::inode(const filesystem &fs, u32 index) : m_index(index) {
  m_fsid = fs.id();
}

fs::vnoderef fs::inode::reify(void) { return {}; }
