#pragma once

#include <dev/device.h>
#include <ptr.h>
#include <string.h>
#include <types.h>


#define MAX_DRIVERS 255


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

int register_driver(major_t major, unique_ptr<dev::driver>);
int deregister_driver(major_t major);

int register_name(string name, major_t major, minor_t minor);
int deregister_name(string name);

ref<dev::device> open(string name);
ref<dev::device> open(major_t, minor_t);
ref<dev::device> open(major_t, minor_t, int &errcode);

};  // namespace dev
