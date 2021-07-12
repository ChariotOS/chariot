#pragma once
#include <types.h>

namespace vmware {

#define VMMOUSE_GETVERSION 10
#define VMMOUSE_DATA 39
#define VMMOUSE_STATUS 40
#define VMMOUSE_COMMAND 41


#define CMD_ABSPOINTER_DATA 39
#define CMD_ABSPOINTER_STATUS 40
#define CMD_ABSPOINTER_COMMAND 41

#define ABSPOINTER_ENABLE 0x45414552 /* Q E A E */
#define ABSPOINTER_RELATIVE 0xF5
#define ABSPOINTER_ABSOLUTE 0x53424152 /* R A B S */

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
