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
  INFO("driver '%s' created\n", name());
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

/**
 * the internal structure of devices
 */
struct driver_instance {
  string name;
  int type;
  major_t major;
  dev::driver_ops *ops;
};

// every device drivers, accessable through their major number
static map<major_t, struct driver_instance> device_drivers;

int dev::register_driver(const char *name, int type, major_t major, dev::driver_ops *d) {
  // TODO: take a lock

  if (d == nullptr) return -ENOENT;

  if (major > MAX_DRIVERS) return -E2BIG;

  if (device_drivers.contains(major)) {
    return -EEXIST;
  }

  struct driver_instance inst;
  inst.name = name;
  inst.type = type; // TODO: assert the type is valid
  inst.major = major;
  inst.ops = d;

  device_drivers[major] = move(inst);

  return 0;
}

int dev::deregister_driver(major_t major) {
  // TODO: take a lock

  if (major > MAX_DRIVERS) return -E2BIG;

  if (device_drivers.contains(major)) {
    // TODO: call driver::deregister() or something
    device_drivers.remove(major);
    return 0;
  }

  return -ENOENT;
}

map<string, dev_t> device_names;

int dev::register_name(string name, major_t major, minor_t minor) {
  // TODO: take a lock
  if (device_names.contains(name)) return -EEXIST;
  device_names[name] = {major, minor};

  KINFO("register dev %s to %d:%d (%d devices total)\n", name.get(), major, minor, device_names.size());
  return 0;
}

int dev::deregister_name(string name) {
  // TODO: take a lock
  if (device_names.contains(name)) device_names.remove(name);
  return -ENOENT;
}

// main API to opening devices

fs::filedesc dev::open(string name) {
  int err;

  if (device_names.contains(name)) {
    auto d = device_names.get(name);
    return dev::open(d.major(), d.minor(), err);
  }

  return fs::filedesc(NULL, 0);
}

fs::filedesc dev::open(major_t maj, minor_t min) {
  int err;
  auto dev = open(maj, min, err);
  if (err != 0)
    printk(
        "[DRIVER:ERR] open device %d:%d failed with unhandled error code %d\n",
        maj, min, -err);
  return dev;
}

fs::filedesc dev::open(major_t maj, minor_t min, int &errcode) {
  // TODO: is this needed?
  if (maj > MAX_DRIVERS) {
    errcode = -E2BIG;
    return fs::filedesc(NULL, 0);
  }

  // TODO: take a lock!
  if (device_drivers.contains(maj)) {
    errcode = 0;

    auto d = device_drivers[maj];

    auto ino = new fs::inode(d.type == BLOCK_DRIVER ? T_BLK : T_CHAR);
    fs::inode::acquire(ino);
    ino->major = maj;
    ino->minor = min;
    return fs::filedesc(ino, FDIR_READ | FDIR_WRITE);
    // return device_drivers[maj].ops->open(maj, min, errcode);
  }
  // if there wasnt a driver in the major list, return such
  errcode = -ENOENT;
  return fs::filedesc(NULL, 0);
}

dev::driver_ops *dev::get(major_t majr) {
  // TODO: take a lock
  if (device_drivers.contains(majr)) {
    return device_drivers[majr].ops;
  }
  return NULL;
}




static int disk_count = 0;
string dev::next_disk_name(void) {
  return string::format("disk%d", disk_count);
}
