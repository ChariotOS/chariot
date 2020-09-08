#ifndef __BLK_DEV_H__
#define __BLK_DEV_H__

#include <dev/device.h>
#include <dev/driver.h>
#include <ptr.h>

namespace dev {
class disk : public refcounted<disk> {
 public:
  disk();

  virtual ~disk();

  virtual size_t block_size() = 0;
	virtual size_t block_count() = 0;

  // all block devices must implement these functions
  virtual bool read_blocks(u32 index, u8* buf, int n = 1) = 0;
  virtual bool write_blocks(u32 index, const u8* buf, int n = 1) = 0;
};
};  // namespace dev

#endif
