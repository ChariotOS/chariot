#ifndef __CHAR_DEV_H__
#define __CHAR_DEV_H__

#include <dev/device.h>
#include <dev/driver.h>

namespace dev {
class char_dev : public dev::device {
 public:
  char_dev(ref<dev::driver> dr);
  virtual ~char_dev();
  inline virtual bool is_char_device(void) override { return true; }

  // simple API for char devices
  virtual int read(u64 offset, u32 len, void*) override;
  virtual int write(u64 offset, u32 len, const void*) override;
};
};  // namespace dev

#endif
