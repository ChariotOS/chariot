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

	for (auto & dev : all_devices) {
		KINFO("%s\n", dev->name().get());
	}
	return 0;
}


// Register a device with the global device system
void dev::Device::add(ck::string name, ck::ref<Device> dev) {
  dev->set_name(name);

	if (auto mmio = dev->cast<MMIODevice>()) {
  	DEVLOG(GRN "%s", name.get());
		printk(GRY "@" YEL "%08x", mmio->address());
		for (auto &compat : mmio->compat()) {
			printk(GRY " '%s'" RESET, compat.get()); 
		}
		printk("\n");
	} else {
  	DEVLOG("%s\n", name.get());
	}

  scoped_lock l(all_devices_lock);
  assert(all_devices.find(dev).is_end());
  all_devices.push(dev);

	dev::Driver::probe_all(dev);
  // TODO: probe all drivers
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



bool dev::PCIDevice::is_device(uint16_t vendor, uint16_t device) const { return vendor_id == vendor && device_id == device; }
