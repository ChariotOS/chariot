#pragma once

#include <dev/device.h>
#include <fs.h>
#include <ptr.h>
#include <string.h>
#include <types.h>
#include <map.h>

#define MAX_MAJOR 255

namespace dev {

#define DRIVER_INVALID -1
#define DRIVER_CHAR 1
#define DRIVER_BLOCK 2

#define EVENT_PCI_CHANGE 1

  /**
   * this struct is a general information source for the driver subsystem. Must be
   * defined statically in each driver, and will be passed to the register_driver
   * function. The driver subsystem will keep this information
   */
  struct driver_info {
    // fill in all the information below as needed.
    const char *name = NULL;
    int type = DRIVER_INVALID;
    int major = -1;

    // useful for things like when a device is unplugged, or is mounted
    int (*event_handler)(int minor, int ev, void *data);

    // one of these is REQUIRED, based on the type of the driver. (which is also
    // needed)
    union {
      fs::block_operations *block_ops;
      fs::file_operations *char_ops;
    };

    // private to the driver subsystem, do not fiddle with (write to) stuff below here
    rwlock lock;

    // TODO: figure out char devices
    ck::map<minor_t, fs::blkdev *> block_devices;

    // ...
  };

  void populate_inode_device(fs::inode &);

  int register_driver(struct driver_info &);
  int deregister_driver(struct driver_info &);

  int register_name(struct driver_info &, string name, minor_t minor);
  int deregister_name(struct driver_info &, string name);

  /* return the next diskN where n is the next disk number
   * ie: disk1, disk2, disk3
   */
  string next_disk_name(void);

  // useful functions for the kernel to access devices by name or maj/min
  ck::ref<fs::file> open(string name);

};  // namespace dev
