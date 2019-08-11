#ifndef __INODE_H__
#define __INODE_H__

#include <types.h>

namespace fs {

// forward decl
class filesystem;

class inode {
 public:
  virtual ~inode();

 protected:
  inode(filesystem &fs, u32 index);

 private:
  filesystem &m_fs;
  u32 m_index = 0;
};

/**
 * inode_id - used to locate an inode without sending the inode structure
 *
 * @m_fsid: the filesystem identifier. Used to find the FS
 * @m_index: the inode index inteh filesystem pointed to by m_fsid
 */
class inode_id {
 private:
  u32 m_fsid = 0;
  u32 m_index = 0;

 public:
  inline bool operator==(const inode_id &other) const {
    return m_fsid == other.m_fsid && m_index == other.m_index;
  }

  inline bool operator!=(const inode_id &other) const {
    return !operator==(other);
  }

  bool is_root_inode(void) const;

  // return the filesystem this inode live in
  filesystem *fs();
  const filesystem *fs() const;
};
}  // namespace fs

#endif
