#ifndef __VNODE_H__
#define __VNODE_H__

#include <ptr.h>
#include <string.h>
#include <types.h>

namespace fs {

// forward decl
class filesystem;
struct inode_metadata;

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
  off_t size = 0;
  inode_type type;
  u32 mode = 0;
  u32 uid = 0;
  u32 gid = 0;
  u32 link_count = 0;
  time_t atime = 0;
  time_t ctime = 0;
  time_t mtime = 0;
  time_t dtime = 0;
  u32 block_count = 0;
  u32 block_size = 0;
};

/**
 * a vnode is a virtual inode.
 *
 * Each filesystem implements their own vnode for their use cases.
 */
class vnode {
 public:
  virtual ~vnode();

  inline u32 index(void) const { return m_index; }

  // return the three octal permission mode
  inline virtual u16 permissions(void) { return 0777; }

  inline filesystem &fs(void) { return m_fs; }

  inline off_t size(void) { return metadata().size; }
  inline bool is_simlink(void) {
    return metadata().type == inode_type::symlink;
  }
  inline bool is_dir(void) { return metadata().type == inode_type::dir; }
  inline u32 mode(void) { return metadata().mode; }

  virtual inode_metadata metadata(void) = 0;



  // add a file to the directory that this vnode points to
  // Error examples:
  //  -ENOTDIR - not a directory
  //  -EPERM - operation not permitted
  //  -ENAMETOOLONG - name is too long for the particular filesystem
  //  -... - use your brain
  virtual int add_dir_entry(ref<vnode> node, const string &name, u32 mode) = 0;

 protected:
  vnode(filesystem &fs, u32 index);

 private:
  filesystem &m_fs;
  u32 m_index = 0;
};

using vnoderef = ref<vnode>;

// an inode is a simple struct that just contains the fs_id and the inode index
// within that filesystem. it is mostly used internally when referencing an
// inode without a backing datastructure. For example, when walking a directory,
// you may not need to create the full-on vnode structure on the heap when all
// you need is a simple inode index.
//
// Because of this, inodes need to be able to lookup or 'reify' themselves into
// the full-on vnode.
class inode {
 public:
  inode(u32 fsid, u32 index);
  inode(const filesystem &fs, u32 index);

  // will panic if the filesystem is not found
  filesystem &fs(void);
  u32 index(void);
  // turn get the vnode for this inode
  vnoderef reify(void);

 private:
  u32 m_fsid;
  u32 m_index;
};

}  // namespace fs

#endif
