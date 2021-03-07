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
  inline dev_t() : dev_t(0, 0) {
  }
  inline dev_t(major_t maj, minor_t min) : maj(maj), min(min) {
  }

  inline major_t major(void) {
    return maj;
  }
  inline minor_t minor(void) {
    return min;
  }

  inline bool operator==(const dev_t &o) {
    return o.maj == maj && o.min == min;
  }

 protected:
  major_t maj;
  minor_t min;
};

namespace dev {};  // namespace dev

#endif
