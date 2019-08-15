#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include <fs/inode.h>
#include <ptr.h>
#include <string.h>
#include <types.h>
#include <fs/file.h> // fs::path

namespace fs {

class inode;

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

  virtual ref<inode> get_inode(u32 index) = 0;

  // opens a file with 'flags' options
  ref<fs::inode> open(string s, u32 flags);
  virtual ref<fs::inode> open(fs::path, u32 flags) = 0;


  /*
  struct directory_entry {
    directory_entry(const char *name, inode
  };
  */

 protected:
  // TODO: put a lock in here.

  // must be called by subclasses
  filesystem();

 private:
  u32 m_fsid = 0;
  bool m_readonly = false;
};




int mount(string path, filesystem &);
fs::inode open(string s, u32 flags, u32 opts = 0);


}  // namespace fs

#endif
