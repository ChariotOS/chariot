#pragma once

#ifndef __MOBO_DEVMGR_
#define __MOBO_DEVMGR_

namespace mobo {

class device_manager {};

struct device_info {
  char* name;
  int (*init)(device_manager*);
};

};  // namespace mobo

#define mobo_device_register(name, init_dev_fn)                                \
  static char _mobo_device_name[] = name;                                      \
  static struct mobo::device_info _mobo_device __attribute__((__used__))       \
      __attribute__(                                                           \
          (unused, __section__("_mobo_devices"), aligned(sizeof(void*)))) = { \
          _mobo_device_name, init_dev_fn};

#endif
