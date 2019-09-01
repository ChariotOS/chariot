#ifndef __BLK_DEV_H__
#define __BLK_DEV_H__

#include <dev/device.h>
#include <dev/driver.h>
#include <ptr.h>

namespace dev {
class blk_dev : public dev::device {
 public:
  blk_dev(ref<dev::driver> dr);

  virtual ~blk_dev();
  inline virtual bool is_blk_device(void) override { return true; }

  virtual int read(u64 offset, u32 len, void*) override;
  virtual int write(u64 offset, u32 len, const void*) override;

  // all block devices must implement these functions
  virtual bool read_block(u32 index, u8* buf) = 0;
  virtual bool write_block(u32 index, const u8* buf) = 0;


};
};  // namespace dev

#endif
