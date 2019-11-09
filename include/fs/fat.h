#pragma once

#include <dev/blk_cache.h>
#include <dev/blk_dev.h>
#include <fs/filesystem.h>
#include <fs/vnode.h>
#include <map.h>

#include "../../src/fs/fat/ff.h"

namespace fs {

class fat;  // fwd decl


class fat final : public filesystem {
 public:
  // constructor takes the device to use (ideally block device)
  fat(ref<dev::device>);
  ~fat(void);

  virtual bool init(void);

  virtual vnoderef get_root_inode(void);
  virtual vnoderef get_inode(u32 index);
  inline virtual u64 block_size(void) { return blocksize; }

  FATFS fs;

 private:
  // fat does not have inodes... so we have to "emulate" them
  int next_inode = 1;
  map<int, vnoderef> inodes;

  int device_id = 0;
  int blocksize = 512;
};
};  // namespace fs
