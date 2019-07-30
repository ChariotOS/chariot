#include <mobo/dev_mgr.h>

// extern mobo::device_info __start__mobo_devices[];
// extern mobo::device_info __stop__mobo_devices[];

using namespace mobo;

static std::vector<device_registration_manager *> device_regstrations;

device_registration_manager::device_registration_manager(
    const char *name, std::function<device *()> creator) {
  this->name = name;
  this->creator = creator;
  device_regstrations.push_back(this);
}

void device_manager::reset(void) {
  devices.clear();
  mmio_devices.clear();
  io_handlers.clear();
  for (auto dr : device_regstrations) {
    device *d = dr->creator();
    add_device(d);
  }
}

void device_manager::add_device(device *d) {
  // device *d = dev->create();
  auto ports = d->get_ports();
  d->init();
  for (auto p : ports) {
    io_handlers[p] = d;
  }
  devices.push_back(d);
}

device_manager::device_manager() {
  reset();
}

int device_manager::handle_io(vcpu *cpu, port_t port, bool in, void *data,
                              uint32_t len) {
  if (io_handlers.count(port) == 0) {
    // printf("unhandled port 0x%04x\n", port);
    return 0;
  }

  auto *dev = io_handlers[port];

  if (in) return dev->in(cpu, port, len, data);
  return dev->out(cpu, port, len, data);
}

/**
 * Some non-overloaded default functions for the device class
 */
void device::init(void) {}
std::vector<port_t> device::get_ports() { return {}; }
int device::in(mobo::vcpu *, port_t, int, void *) { return 0; }
int device::out(mobo::vcpu *, port_t, int, void *) { return 0; }

std::vector<device::memory_range> device::get_mmio_ranges(void) { return {}; }
bool device::read(mobo::vcpu *, uint64_t addr, void *) { return false; }
bool device::write(mobo::vcpu *, uint64_t addr, void *) { return false; }
