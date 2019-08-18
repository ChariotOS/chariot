#include <fs/filesystem.h>
#include <fs/inode.h>

fs::vnode::vnode(fs::filesystem &fs, u32 index) : m_fs(fs), m_index(index) {}
fs::vnode::~vnode(void) {}

fs::inode::inode(u32 fsid, u32 index) : m_fsid(fsid), m_index(index) {}

// get the filesystem information from an actual filesystem
fs::inode::inode(const filesystem &fs, u32 index) : m_index(index) {
  m_fsid = fs.id();
}

fs::vnoderef fs::inode::reify(void) { return {}; }
