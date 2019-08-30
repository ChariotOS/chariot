#include <fs/filesystem.h>
#include <fs/vfs.h>
#include <fs/vnode.h>

fs::vnode::vnode(fs::filesystem &fs, u32 index) : m_fs(fs), m_index(index) {}
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

  // call the inode implementation of walk_dir, but stand between and handle
  // mount points
  return walk_dir_impl([&](const string &s, u32 in) -> bool {
    // the dir walker lets you stop walking by returning false, so we need to
    // keep track of that
    bool cont = false;

    auto qual = vfs::qualified_inode_number(fs(), in);

    if (s == dot) {
      printk("dot!\n");
    }

    if (s == dotdot) {
      printk("dotdot!\n");
    }

    if (vfs::get_mount_at(qual)) {
      printk("is mount point\n");
    }

    // TODO: handle if vn->index is a mount point
    if (/*isnt mount point*/ true) {
      auto vn = fs().get_inode(in);
      cont = cb(s, fs().get_inode(in));
    } else {
    }

    return cont;
  });
}

u8 *fs::vnode::read_entire(void) {
  auto sz = size();
  auto buf = kmalloc(sz);
  read(0, sz, buf);
  return (u8 *)buf;
}

// get the filesystem information from an actual filesystem
fs::inode::inode(const filesystem &fs, u32 index) : m_index(index) {
  m_fsid = fs.id();
}

fs::vnoderef fs::inode::reify(void) { return {}; }
