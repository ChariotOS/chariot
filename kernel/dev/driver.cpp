#include <dev/driver.h>
#include <errno.h>
#include <ck/map.h>
#include <printk.h>
#include <fs/vfs.h>
#include <module.h>
#include "fs.h"

#define LOG(...) PFXLOG(BLU "DRV", __VA_ARGS__)

#define DRIVER_REFACTOR() KWARN("fs refactor: old driver function '%s' called.\n", __PRETTY_FUNCTION__)

static int next_major = 0;
static rwlock drivers_lock;

int dev::register_driver(struct dev::DriverInfo &info) {
  KWARN("Driver %s registered. Please rewrite, as it will not be visible in the filesystem.\n", info.name);
  return 0;
}

extern void devfs_register_device(ck::string name, int type, int major, int minor);
int dev::register_name(struct dev::DriverInfo &info, ck::string name, minor_t min) {
  DRIVER_REFACTOR();
  return 0;
}

int dev::deregister_name(struct dev::DriverInfo &, ck::string name) {
  DRIVER_REFACTOR();
  return -ENOENT;
}


static int disk_count = 0;
ck::string dev::next_disk_name(void) { return ck::string::format("disk%d", disk_count); }


static void device_irq_handler(int num, reg_t *regs, void *data) {
  dev::Device *self = (dev::Device *)data;
  self->irq(num);
}

void dev::Device::handle_irq(int num, const char *name) { irq::install(num, device_irq_handler, name, this); }




static spinlock all_drivers_lock;
static ck::vec<ck::ref<dev::Driver>> all_drivers;


dev::Driver::Driver(void) { m_major = next_major++; }

auto dev::Driver::probe(ck::ref<hw::Device> device) -> dev::ProbeResult {
  // By default, Ignore the device.
  return dev::ProbeResult::Ignore;
}


static void probe(ck::ref<hw::Device> dev, ck::ref<dev::Driver> drv) {
  if (dev->driver() != nullptr) return;
  if (drv->probe(dev) == dev::ProbeResult::Attach) {
    drv->attach(dev);
  }
}


static void probe_all_r(ck::ref<hw::Device> dev, ck::ref<dev::Driver> drv = nullptr) {
  if (drv == nullptr) {
    for (auto drv : all_drivers) {
      probe(dev, drv);
    }
  } else {
    probe(dev, drv);
  }

  for (auto c : dev->children()) {
    probe_all_r(c);
  }
}

void dev::Driver::add(ck::ref<dev::Driver> driver) {
  scoped_lock l(all_drivers_lock);

  all_drivers.push(driver);
  for (auto dev : hw::Device::all()) {
    probe_all_r(dev, driver);
  }
}



void dev::Driver::probe_all(ck::ref<hw::Device> dev) {
  scoped_lock l(all_drivers_lock);
  probe_all_r(dev, nullptr);
}


ksh_def("drivers", "display all (old style) drivers") {
  scoped_lock l(all_drivers_lock);

  for (auto d : all_drivers) {
    printk(" - %s\n", d->name().get());
  }
  return 0;
}
