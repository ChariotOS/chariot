#ifndef __INODE_H__
#define __INODE_H__

#include <types.h>

namespace fs {

// forward decl
class filesystem;

class inode {
 public:
  virtual ~inode();

  inline u32 index(void) const { return m_index; }

 protected:
  inode(filesystem &fs, u32 index);

 private:
  filesystem &m_fs;
  u32 m_index = 0;
};

}  // namespace fs

#endif
