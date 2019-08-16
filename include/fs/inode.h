#ifndef __INODE_H__
#define __INODE_H__

#include <ptr.h>
#include <types.h>

namespace fs {

// forward decl
class filesystem;
struct inode_metadata;

class inode {
 public:
  virtual ~inode();

  inline u32 index(void) const { return m_index; }

  // return the three octal permission mode
  inline virtual u16 permissions(void) { return 0777; }

  inline filesystem &fs(void) { return m_fs; }

  virtual inode_metadata metadata(void) = 0;

 protected:
  inode(filesystem &fs, u32 index);

 private:
  filesystem &m_fs;
  u32 m_index = 0;
};

struct inode_id {};

enum class inode_type : u8 {
  unknown = 0,
  fifo,
  char_dev,
  dir,
  block_dev,
  file,
  symlink,
  unix_socket,
};

struct inode_metadata {
  inode_id inode;
  off_t size{0};
  inode_type type;
  u32 mode{0};
  u32 uid{0};
  u32 gid{0};
  u32 link_count{0};
  time_t atime{0};
  time_t ctime{0};
  time_t mtime{0};
  time_t dtime{0};
  u32 block_count{0};
  u32 block_size{0};
  unsigned major_device{0};
  unsigned minor_device{0};
};

}  // namespace fs

#endif
