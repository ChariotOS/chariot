#pragma once
#include <types.h>

namespace vmware {

#define VMMOUSE_GETVERSION 10
#define VMMOUSE_DATA 39
#define VMMOUSE_STATUS 40
#define VMMOUSE_COMMAND 41

  struct command {
    union {
      uint32_t ax;
      uint32_t magic;
    };
    union {
      uint32_t bx;
      uint32_t size;
    };
    union {
      uint32_t cx;
      uint32_t cmd;
    };
    union {
      uint32_t dx;
      uint32_t port;
    };
    uint32_t si;
    uint32_t di;
  };


  bool supported();
  bool vmmouse_is_absolute();
  void enable_absolute_vmmouse();
  void disable_absolute_vmmouse();
  void send(struct vmware::command& command);


};  // namespace vmware
