#pragma once

#include <dev/device.h>
#include <ptr.h>
#include <string.h>
#include <types.h>

/**
 * a major number represents the device driver ID
 */
using major_t = u16;
using minor_t = u16;

#define MAX_DRIVERS 255

struct dev_t {
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

// a driver is an interface to opening devices using a major
// number and a minor number
class driver : public refcounted<driver> {
 public:
  driver();
  virtual ~driver();

  ref<dev::device> open(major_t, minor_t);
  virtual ref<dev::device> open(major_t, minor_t, int &errcode);

  virtual int release(dev::device *);

  inline virtual const char *name(void) const { return "unknown"; }
};

int register_driver(major_t major, ref<dev::driver>);
int deregister_driver(major_t major);

ref<dev::device> open(major_t, minor_t);
ref<dev::device> open(major_t, minor_t, int &errcode);

};  // namespace dev
