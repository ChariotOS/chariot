#pragma once

#include <dev/blk_cache.h>
#include <dev/blk_dev.h>
#include <fs/filesystem.h>
#include <fs/vnode.h>

#include "../../src/fs/fat/ff.h"

namespace fs {

class fat;  // fwd decl

class fat_inode : public vnode {
  friend class ext2;

 public:
  explicit fat_inode(fs::fat &fs, FIL *file);
  virtual ~fat_inode();

  virtual file_metadata metadata(void);

  virtual int add_dir_entry(ref<vnode> node, const string &name, u32 mode);
  virtual ssize_t read(filedesc &, void *, size_t);
  virtual ssize_t write(filedesc &, void *, size_t);
  virtual int truncate(size_t newlen);

  // return the block size of the fs
  int block_size();

  inline virtual fs::filesystem &fs() { return m_fs; }

 protected:
  int cached_path[4] = {0, 0, 0, 0};
  int *blk_bufs[4] = {NULL, NULL, NULL, NULL};

  ssize_t do_rw(fs::filedesc &, size_t, void *, bool is_write);
  off_t block_for_byte(off_t b);

  // return the ith block's index, returning 0 on failure.
  // if set_to is passed and is non-zero, the block at that
  // index will be written as set_to
  virtual bool walk_dir_impl(func<bool(const string &, u32)> cb);


  FIL *file;
  fs::filesystem &m_fs;
};

class fat final : public filesystem {
 public:
  // constructor takes the device to use (ideally block device)
  fat(ref<dev::device>);
  ~fat(void);

  virtual bool init(void);

  virtual vnoderef get_root_inode(void);
  virtual vnoderef get_inode(u32 index);
  inline virtual u64 block_size(void) { return blocksize; }

 private:
  FATFS *fs = nullptr;

  int device_id = 0;
  int blocksize = 512;
};
};  // namespace fs
