#include <dev/driver.h>
#include <errno.h>
#include <map.h>
#include <printk.h>

#define DRIVER_DEBUG

#ifdef DRIVER_DEBUG
#define INFO(fmt, args...) printk("[DRIVER] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

dev::driver::driver() {
  // INFO("driver '%s' created\n", name());
}

dev::driver::~driver(void) {}

ref<dev::device> dev::driver::open(major_t maj, minor_t min) {
  int err;
  return open(maj, min, err);
}
ref<dev::device> dev::driver::open(major_t, minor_t, int &errcode) {
  errcode = -ENOTIMPL;
  return nullptr;
}

int dev::driver::release(dev::device *) { return -ENOTIMPL; }

static ref<dev::driver> drivers[MAX_DRIVERS];

int dev::register_driver(major_t major, ref<dev::driver> d) {
  // TODO: take a lock

  if (d.get() == nullptr) return -ENOENT;

  if (major > MAX_DRIVERS) return -E2BIG;

  if (drivers[major].get() != nullptr) {
    // if the driver at major is d, then it already exists
    if (drivers[major] == d) return -EEXIST;

    // the major number was different
    return -EBUSY;
  }

  drivers[major] = d;

  return 0;
}

int dev::deregister_driver(major_t major) {
  // TODO: take a lock

  if (major > MAX_DRIVERS) return -E2BIG;

  if (drivers[major].get() == nullptr) return -ENOENT;

  drivers[major] = nullptr;
  return 0;
}

map<string, dev_t> device_names;

int dev::register_name(string name, major_t major, minor_t minor) {
  // TODO: take a lock
  if (device_names.contains(name)) return -EEXIST;
  device_names[name] = {major, minor};
  return 0;
}

int dev::deregister_name(string name) {
  // TODO: take a lock
  if (device_names.contains(name)) device_names.remove(name);
  return -ENOENT;
}


// main API to opening devices

ref<dev::device> dev::open(string name) {

  int err;

  if (device_names.contains(name)) {
    auto d = device_names.get(name);
    return dev::open(d.major(), d.minor(), err);
  }

  return nullptr;
}

ref<dev::device> dev::open(major_t maj, minor_t min) {
  int err;
  auto dev = open(maj, min, err);
  if (err != 0)
    printk(
        "[DRIVER:ERR] open device %d:%d failed with unhandled error code %d\n",
        maj, min, -err);
  return dev;
}

ref<dev::device> dev::open(major_t maj, minor_t min, int &errcode) {
  if (maj > MAX_DRIVERS) {
    errcode = -E2BIG;
    return nullptr;
  }

  // TODO: take a lock!
  if (drivers[maj].get() != nullptr) {
    // forward errcode to the driver's open code
    return drivers[maj]->open(maj, min, errcode);
  }

  // if there wasnt a driver in the major list, return such
  errcode = -ENOENT;
  return nullptr;
}
