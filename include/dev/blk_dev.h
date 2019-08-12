#ifndef __BLK_DEV_H__
#define __BLK_DEV_H__

#include <dev/device.h>

namespace dev {
class blk_dev : public dev::device {
 public:

  virtual ~blk_dev();
  inline virtual bool is_blk_device(void) override { return true; }

  bool read(u64 offset, u32 len, void*);
  bool write(u64 offset, u32 len, const void*);

  // all block devices must implement these functions

  /**
   * return the size of a single block.
   *
   * Example: an ATA driver returns 512
   */
  virtual u64 block_size(void) = 0;
  virtual bool read_block(u32 index, u8* buf) = 0;
  virtual bool write_block(u32 index, const u8* buf) = 0;
};
};  // namespace dev

#endif
