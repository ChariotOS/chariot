#pragma once

#ifndef __MOBO_MACHINE_
#define __MOBO_MACHINE_

#include <stdlib.h>
#include <string>
#include "dev_mgr.h"

namespace mobo {

class driver;

class machine {
  char *memory = nullptr;
  size_t mem_size = 0;

  driver &m_driver;

 public:
  machine(driver &);
  ~machine();
  void load_elf(std::string file);

  // give the machine nbytes bytes of ram, rounded up to nearest
  // page boundary (4096 bytes)
  void allocate_ram(size_t);

  void run(void);
};

// a driver is an interface which acts as a generic boundary API
// to various vm implementations
class driver {
 private:
  machine *m_machine = nullptr;

 protected:
  // every driver has a device manager for port io
  device_manager dev_mgr;

 public:
  virtual ~driver(){};
  void set_machine(machine *);
  virtual void attach_memory(size_t, void *) = 0;

  virtual void load_elf(std::string &) = 0;
  virtual void run(void) = 0;
};
}  // namespace mobo

#endif
