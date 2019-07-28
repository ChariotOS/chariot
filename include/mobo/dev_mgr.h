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
  struct memory_range {
    uint64_t start, end;
  };
  virtual std::vector<port_t> get_ports(void);
  virtual void init(void);

  // Legacy port io handlers
  virtual int in(mobo::vcpu *, port_t, int, void *);
  virtual int out(mobo::vcpu *, port_t, int, void *);

  // MMIO information
  virtual std::vector<memory_range> get_mmio_ranges(void);
  virtual bool read(mobo::vcpu *, uint64_t addr, void *);
  virtual bool write(mobo::vcpu *, uint64_t addr, void *);
};

class device_manager {
  struct mmio_map {
    device::memory_range mmio;
    device *dev;
  };
  std::vector<mmio_map> mmio_devices;
  std::unordered_map<port_t, device *> io_handlers;
  std::vector<device *> devices;

 public:
  device_manager();

  void add_device(device *);

  int handle_io(vcpu *, port_t, bool, void *, uint32_t);
};

struct device_info {
  char *name;
  std::function<device *()> create;
};

int port_io_nop(mobo::port_t, void *, int, void *);

class device_registration_manager {
 public:
  const char *name;
  std::function<device *()> creator;
  device_registration_manager(const char *, std::function<device *()>);
};

};  // namespace mobo

#define MOBO_REGISTER_DEVICE(cls)                                  \
  static char _mobo_device_name[] = #cls;                          \
  static mobo::device_registration_manager _mobo_device            \
      __attribute__((__used__))                                    \
          __attribute__((unused, aligned(sizeof(void *)))) =       \
              mobo::device_registration_manager(_mobo_device_name, \
                                                []() { return new cls(); });

#endif
