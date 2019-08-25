#include <dev/device.h>
#include <dev/blk_dev.h>

dev::blk_dev *dev::device::to_blk_dev(void) {
  if (is_blk_device()) {
    return static_cast<dev::blk_dev*>(this);
  }
  return nullptr;
}
