#include <dev/device.h>
#include <dev/blk_dev.h>
#include <dev/char_dev.h>



dev::device::device(ref<dev::driver> dr) : m_driver(dr) {
}

dev::device::~device() {}

dev::blk_dev *dev::device::to_blk_dev(void) {
  if (is_blk_device()) {
    return static_cast<dev::blk_dev*>(this);
  }
  return nullptr;
}
dev::char_dev *dev::device::to_char_dev(void) {
  if (is_char_device()) {
    return static_cast<dev::char_dev*>(this);
  }
  return nullptr;
}


ref<dev::driver> dev::device::driver(void) {
  return m_driver;
}
