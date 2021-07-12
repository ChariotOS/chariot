#include <asm.h>
#include <cpu.h>
#include <printk.h>
#include <util.h>
#include <vmware_backdoor.h>

#define VMWARE_CMD_GETVERSION 0x0a

#define VMMOUSE_READ_ID 0x45414552
#define VMMOUSE_DISABLE 0x000000f5
#define VMMOUSE_REQUEST_RELATIVE 0x4c455252
#define VMMOUSE_REQUEST_ABSOLUTE 0x53424152

#define VMMOUSE_QEMU_VERSION 0x3442554a

#define VMWARE_MAGIC 0x564D5868
#define VMWARE_PORT 0x5658
#define VMWARE_PORT_HIGHBANDWIDTH 0x5659


inline void vmware_out(struct vmware::command& command) {
  command.magic = VMWARE_MAGIC;
  command.port = VMWARE_PORT;
  command.si = 0;
  command.di = 0;

  // printk("put in:\n");
  // hexdump(&command, sizeof(command), true);

  asm volatile("in %%dx, %0"
               : "+a"(command.ax), "+b"(command.bx), "+c"(command.cx), "+d"(command.dx),
               "+S"(command.si), "+D"(command.di));


  // printk("got out:\n");
  // hexdump(&command, sizeof(command), true);
}

inline void vmware_high_bandwidth_send(struct vmware::command& command) {
  command.magic = VMWARE_MAGIC;
  command.port = VMWARE_PORT_HIGHBANDWIDTH;

  asm volatile("cld; rep; outsb"
               : "+a"(command.ax), "+b"(command.bx), "+c"(command.cx), "+d"(command.dx),
               "+S"(command.si), "+D"(command.di));
}

inline void vmware_high_bandwidth_get(struct vmware::command& command) {
  command.magic = VMWARE_MAGIC;
  command.port = VMWARE_PORT_HIGHBANDWIDTH;
  asm volatile("cld; rep; insb"
               : "+a"(command.ax), "+b"(command.bx), "+c"(command.cx), "+d"(command.dx),
               "+S"(command.si), "+D"(command.di));
}


static bool detect_presence() {
  struct vmware::command command;
  command.bx = ~VMWARE_MAGIC;
  command.cmd = VMWARE_CMD_GETVERSION;
  memset(&command, 0, sizeof(command));
  vmware_out(command);
  if (command.bx != VMWARE_MAGIC || command.ax == 0xFFFFFFFF) return false;
  return true;
}


static bool queried = false;
static bool supported = false;  // we don't know yet

bool vmware::supported(void) {
  if (!queried) {
    ::supported = detect_presence();
    queried = true;
  }

  return ::supported;
}


void vmware::send(struct vmware::command& command) {
  // if (supported()) {
  vmware_out(command);
  // }
}




/*
static bool detect_vmmouse() {
        if (!supported())
                return false;
  struct vmware::command command;
  command.bx = VMMOUSE_READ_ID;
  command.cmd = VMMOUSE_COMMAND;
  send(command);
  command.bx = 1;
  command.cmd = VMMOUSE_DATA;
  send(command);
  if (command.ax != VMMOUSE_QEMU_VERSION) return false;
  return true;
}
*/

static bool mouse_absolute = false;

bool vmware::vmmouse_is_absolute(void) { return mouse_absolute; }

void vmware::enable_absolute_vmmouse(void) {
  if (!supported()) return;


  struct vmware::command command;

  command.bx = 0;
  command.cmd = VMMOUSE_STATUS;
  send(command);
  if (command.ax == 0xFFFF0000) {
    printk("VMMouse retuned bad status.\n");
    return;
  }

  // Enable absolute vmmouse
  command.bx = VMMOUSE_REQUEST_ABSOLUTE;
  command.cmd = VMMOUSE_COMMAND;
  send(command);
  mouse_absolute = true;
}

void vmware::disable_absolute_vmmouse(void) {
  if (!supported()) return;
}
