#pragma once

#include <dev/device.h>
#include <fs.h>
#include <ck/ptr.h>
#include <ck/string.h>
#include <types.h>
#include <ck/map.h>

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
  struct DriverInfo {
    // fill in all the information below as needed.
    const char *name = NULL;
    int type = DRIVER_INVALID;
    int major = -1;

    // useful for things like when a device is unplugged, or is mounted
    int (*event_handler)(int minor, int ev, void *data);

    // one of these is REQUIRED, based on the type of the driver. (which is also
    // needed)
    union {
      fs::BlockOperations *block_ops;
      fs::FileOperations *char_ops;
    };

    // private to the driver subsystem, do not fiddle with (write to) stuff below here
    rwlock lock;

    // TODO: figure out char devices
    ck::map<minor_t, fs::BlockDevice *> block_devices;

    // ...
  };

  void populate_inode_device(fs::Node &);

  int register_driver(struct DriverInfo &);
  int deregister_driver(struct DriverInfo &);

  int register_name(struct DriverInfo &, ck::string name, minor_t minor);
  int deregister_name(struct DriverInfo &, ck::string name);

  /* return the next diskN where n is the next disk number
   * ie: disk1, disk2, disk3
   */
  ck::string next_disk_name(void);

  // useful functions for the kernel to access devices by name or maj/min
  ck::ref<fs::File> open(ck::string name);

};  // namespace dev


namespace dev {
  enum ProbeResult { Attach, Ignore };

  class Driver : public ck::refcounted<Driver> {
   public:
    Driver() {}
    virtual ~Driver(void) {}

    virtual ProbeResult probe(ck::ref<dev::Device> dev);


    static void add(ck::ref<dev::Driver>);
    static void probe_all(ck::ref<dev::Device>);
  };

}  // namespace dev


#define driver_init(name, T)                                        \
  void _MOD_VARNAME(__driver_init)(void) { dev::Driver::add(ck::make_ref<T>()); } \
  module_init(name, _MOD_VARNAME(__driver_init));
