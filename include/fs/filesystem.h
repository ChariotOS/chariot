#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include <fs/file.h>  // fs::path
#include <fs/vnode.h>
#include <ptr.h>
#include <string.h>
#include <types.h>

namespace fs {

using mode_t = u32;

struct directory_entry {
  u32 inode;
  string name;
};

// abstract filesystem class. All filesystems must extend from this
class filesystem {
 public:
  virtual ~filesystem();

  // getters
  inline u32 id() const { return m_fsid; }
  inline bool readonly(void) const { return m_readonly; }

  virtual bool init(void) = 0;

  virtual vnoderef get_inode(u32 index) = 0;
  virtual vnoderef get_root_inode() = 0;
  virtual u64 block_size(void) = 0;

  // signal to the filesystem that the filesystem is unmounting.
  // Return false if the filesystem fails to unmount (mostly useful for
  // synthetic kernel filesystems like devfs)
  inline virtual bool umount(void) { return true; }

 protected:
  // TODO: put a lock in here.

  // must be called by subclasses
  filesystem();

 private:
  u32 m_fsid = 0;
  bool m_readonly = false;
};

int mount(string path, filesystem &);
ref<fs::vnode> open(string s, u32 flags, u32 opts = 0);

}  // namespace fs

#endif
