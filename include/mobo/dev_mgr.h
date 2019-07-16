#pragma once

#ifndef __MOBO_DEVMGR_
#define __MOBO_DEVMGR_

#include <functional>

namespace mobo {

using port_t = uint16_t;
// a port io handler returns the length, takes a port dst len and private data
using port_io_handler_fn = std::function<int(port_t, void *, uint32_t, void *)>;

struct io_handler {
  uint16_t port;
  port_io_handler_fn in;
  port_io_handler_fn out;
  void *data = nullptr;  // data is user data, whatever they want
};

class device_manager {
  std::unordered_map<port_t, io_handler> io_handlers;

 public:
  device_manager();
  void hook_io(port_t, port_io_handler_fn, port_io_handler_fn,
               void *data = nullptr);

  int handle_io(port_t, bool, void *, uint32_t);
};

struct device_info {
  char *name;
  int (*init)(device_manager *);
};

};  // namespace mobo

#define mobo_device_register(name, init_dev_fn)                                \
  static char _mobo_device_name[] = name;                                      \
  static struct mobo::device_info _mobo_device __attribute__((__used__))       \
      __attribute__(                                                           \
          (unused, __section__("_mobo_devices"), aligned(sizeof(void *)))) = { \
          _mobo_device_name, init_dev_fn};

#endif
