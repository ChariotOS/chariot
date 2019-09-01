#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <ptr.h>
#include <types.h>

/**
 * a major number represents the device driver ID
 */
using major_t = u16;
using minor_t = u16;

struct dev_t {
  inline dev_t() : dev_t(0, 0) {}
  inline dev_t(major_t maj, minor_t min) : maj(maj), min(min) {}

  inline major_t major(void) { return maj; }
  inline minor_t minor(void) { return min; }

  inline bool operator==(const dev_t &o) {
    return o.maj == maj && o.min == min;
  }

 protected:
  major_t maj;
  minor_t min;
};

namespace dev {

class blk_dev;
class char_dev;

class driver;

class device : public refcounted<device> {
 public:
  device(ref<dev::driver> dr);
  virtual ~device();
  inline virtual bool is_blk_device(void) { return false; }
  inline virtual bool is_char_device(void) { return false; }

  blk_dev *to_blk_dev(void);
  char_dev *to_char_dev(void);

  virtual int read(u64 offset, u32 len, void *) = 0;
  virtual int write(u64 offset, u32 len, const void *) = 0;

  ref<dev::driver> driver();

 private:
  ref<dev::driver> m_driver;
};

};  // namespace dev

#endif
