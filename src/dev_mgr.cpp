#include <mobo/dev_mgr.h>

extern mobo::device_info __start__mobo_devices[];
extern mobo::device_info __stop__mobo_devices[];
using namespace mobo;

device_manager::device_manager() {
  mobo::device_info *dev = __start__mobo_devices;

  int i = 0;
  while (dev != __stop__mobo_devices) {
    if (dev->init != nullptr) {
      dev->init(this);
    }
    dev = &(__start__mobo_devices[++i]);
  }
}

void device_manager::hook_io(port_t port, port_io_handler_fn in,
                             port_io_handler_fn out, void *data) {
  // just a simple hashmap assignment
  io_handlers[port] = {.port = port, .in = in, .out = out, .data = data};
}

int device_manager::handle_io(port_t port, bool in, void *data, uint32_t len) {
  if (io_handlers.count(port) == 0) return 0;

  auto &handle = io_handlers[port];

  if (in) return handle.in(port, data, len, handle.data);
  return handle.out(port, data, len, handle.data);
}

// This is a butchering of the section system, but it works
int nothing_device_init(mobo::device_manager *man) { return 0; }
mobo_device_register("PLACEHOLDER", nullptr)

