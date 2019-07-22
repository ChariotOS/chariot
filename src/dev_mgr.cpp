#include <mobo/dev_mgr.h>

extern mobo::device_info __start__mobo_devices[];
extern mobo::device_info __stop__mobo_devices[];
using namespace mobo;

device_manager::device_manager() {
  mobo::device_info *dev = __start__mobo_devices;
  int i = 0;
  while (dev != __stop__mobo_devices) {
    device *d = dev->create();

    auto ports = d->get_ports();

    for (auto p : ports) {
      io_handlers[p] = d;
    }

    devices.push_back(d);
    dev = &(__start__mobo_devices[++i]);
  }
}

int device_manager::handle_io(vcpu * cpu, port_t port, bool in, void *data, uint32_t len) {
  if (io_handlers.count(port) == 0) {
    printf("unhandled port 0x%04x\n", port);
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

