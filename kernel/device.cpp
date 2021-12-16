#include <dev/device.h>
#include <dev/driver.h>
#include <lock.h>
#include <ck/vec.h>
#include <module.h>



#define DEVLOG(...) PFXLOG(YEL "DEV", __VA_ARGS__)

static spinlock all_devices_lock;
static ck::vec<ck::ref<dev::Device>> all_devices;

dev::Device::Device(DeviceType t) : m_type(t) {}


ksh_def("devices", "display all devices") {
  scoped_lock l(all_devices_lock);

  for (auto &dev : all_devices) {
    KINFO("%s\n", dev->name().get());
  }
  return 0;
}

static void recurse_print(ck::ref<dev::Device> dev, int depth = 0) {
  auto spaces = [&] {
    printk(YEL "DEV" RESET ":");
    for (int i = 0; i < depth; i++)
      printk(".");
  };

	spaces();
  if (auto mmio = dev->cast<dev::MMIODevice>()) {
    printk(GRN "%s", dev->name().get());
    printk(GRY "@" YEL "%08x", mmio->address());
    for (auto &compat : mmio->compat()) {
      printk(GRY " '%s'" RESET, compat.get());
    }
    printk("\n");
  } else {
    DEVLOG("%s\n", dev->name().get());
  }

  for (auto &c : dev->children()) {
    recurse_print(c, depth + 1);
  }
}

static void recurse_probe(ck::ref<dev::Device> dev, int depth = 0) {
  dev->lock();


  for (auto &c : dev->children()) {
    recurse_probe(c, depth + 1);
  }
}

// Register a device at the top level
void dev::Device::add(ck::string name, ck::ref<Device> dev) {
  dev->set_name(name);

  recurse_print(dev);


  scoped_lock l(all_devices_lock);
  assert(all_devices.find(dev).is_end());
  all_devices.push(dev);

  dev::Driver::probe_all(dev);
}


// Register a device with the global device system
void dev::Device::remove(ck::ref<Device> dev, RemovalReason reason) {
  scoped_lock l(all_devices_lock);
  // TODO: Notify drivers that the device has been removed, and why
  all_devices.remove_first_matching([dev](auto other) {
    return other == dev;
  });
}

ck::vec<ck::ref<dev::Device>> dev::Device::all(void) {
  scoped_lock l(all_devices_lock);
  ck::vec<ck::ref<dev::Device>> all = all_devices;

  return all;
}


void dev::Device::add_property(ck::string name, dev::DeviceProperty &&prop) {
  assert(!m_locked);
  m_props.set(name, move(prop));
}


ck::option<ck::string> dev::Device::get_prop_string(const ck::string &name) {
  if (m_props.contains(name)) {
    auto &prop = m_props.get(name);
    ck::string val = ck::string((char *)prop.data.data(), prop.data.size());
    return val;
  }
  return {};
}


ck::option<uint64_t> dev::Device::get_prop_int(const ck::string &name) {
  if (m_props.contains(name)) {
    auto &prop = m_props.get(name);
    uint64_t val = 0;
    if (prop.data.size() == 4) {
      val = *(uint32_t *)prop.data.data();
    } else if (prop.data.size() == 8) {
      val = *(uint64_t *)prop.data.data();
    } else {
      printk(KERN_WARN "prop '%s' could not be converted to an int of size 4 or 8\n", name.get());
    }
    return val;
  }
  return {};
}

bool dev::PCIDevice::is_device(uint16_t vendor, uint16_t device) const { return vendor_id == vendor && device_id == device; }
