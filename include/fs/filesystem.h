#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include <fs/file.h>
#include <fs/vnode.h>
#include <ptr.h>
#include <string.h>
#include <types.h>
#include <fs.h>


namespace fs {

using mode_t = u32;

struct directory_entry {
  u32 inode;
  string name;
};

class filesystem {
 public:
  virtual ~filesystem();

  virtual bool init(void) = 0;
  virtual struct inode *get_root() = 0;
  inline virtual bool umount(void) { return true; }

 protected:
  // TODO: put a lock in here.

  // must be called by subclasses
  filesystem();
};

int mount(string path, filesystem &);

struct inode *open(string s, u32 flags, u32 opts = 0);

}  // namespace fs

#endif
