#ifndef __VNODE_H__
#define __VNODE_H__

#include <func.h>
#include <ptr.h>
#include <string.h>
#include <types.h>
#include <errno.h>
#include <dev/device.h>
#include <fs/file.h>

namespace fs {

// forward decl
class filesystem;

/**
 * a vnode is a virtual inode.
 *
 * Each filesystem implements their own vnode for their use cases.
 */
class vnode : virtual public file {
 public:
  virtual ~vnode();

  inline u32 index(void) const { return m_index; }



  // return the three octal permission mode
  inline virtual u16 permissions(void) { return 0777; }

  virtual filesystem &fs(void) = 0;

  inline off_t size(void) { return metadata().size; }
  inline bool is_simlink(void) {
    return metadata().type == file_type::symlink;
  }
  inline bool is_dir(void) { return metadata().type == file_type::dir; }
  inline u32 mode(void) { return metadata().mode; }

  virtual file_metadata metadata(void) = 0;

  // create a directory and a file. Defaulting to no mode (permissions)
  inline virtual ref<vnode> mkdir(string name, u32 mode = 0000) { return {}; };
  inline virtual ref<vnode> touch(string name, fs::file_type t = fs::file_type::file, u32 mode = 0000) { return {}; };

  /*
   * Cause a regular file to be truncated to the size of precisely length bytes.
   *
   * If the file previously was larger than this size, the extra data is lost.
   * If the file previously was shorter, it is extended, and the extended part
   * reads as null bytes ('\0').
   */
  virtual int truncate(size_t len) = 0;

  bool walk_dir(func<bool(const string &, ref<vnode>)> cb);

  // add a file to the directory that this vnode points to
  // Error examples:
  //  -ENOTDIR - not a directory
  //  -EPERM - operation not permitted
  //  -ENAMETOOLONG - name is too long for the particular filesystem
  //  -... - use your brain
  virtual int add_dir_entry(ref<vnode> node, const string &name, u32 mode) = 0;


 protected:
  vnode(u32 index);

  virtual bool walk_dir_impl(func<bool(const string &, u32)> cb) = 0;

 private:
  // filesystem &m_fs;
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
