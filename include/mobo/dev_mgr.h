#pragma once

#ifndef __MOBO_DEVMGR_
#define __MOBO_DEVMGR_

#include <functional>
#include <vector>

namespace mobo {

class vcpu;

using port_t = uint16_t;

class device {
 public:
  virtual std::vector<port_t> get_ports(void);
  // initialize... duh
  virtual void init(void);
  virtual int in(mobo::vcpu *, port_t, int, void *);
  virtual int out(mobo::vcpu *, port_t, int, void *);
};

class device_manager {
  std::unordered_map<port_t, device *> io_handlers;
  std::vector<device *> devices;

 public:
  device_manager();

  int handle_io(vcpu *, port_t, bool, void *, uint32_t);
};

struct device_info {
  char *name;
  std::function<device *()> create;
};

int port_io_nop(mobo::port_t, void *, int, void *);

};  // namespace mobo

#define MOBO_REGISTER_DEVICE(cls)                                              \
  static char _mobo_device_name[] = #cls;                                      \
  static struct mobo::device_info _mobo_device __attribute__((__used__))       \
      __attribute__(                                                           \
          (unused, __section__("_mobo_devices"), aligned(sizeof(void *)))) = { \
          _mobo_device_name, []() { return new cls(); }};

#endif
