#pragma once

#include <dev/hardware.h>
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

  using Prober = dev::ProbeResult (*)(ck::ref<hw::Device>);


  class Module : public ck::refcounted<Module> {
    ck::string m_name;

   public:
    Module() {}
    virtual ~Module(void) {}

    virtual ProbeResult probe(ck::ref<hw::Device> dev);

    // called when probe returns Attach
    virtual void attach(ck::ref<hw::Device> dev) {}



    inline void set_name(ck::string name) { m_name = move(name); }
    inline auto name(void) const { return m_name; }

    static void add(ck::ref<dev::Module>);
    static void probe_all(ck::ref<hw::Device>);
  };


  template <typename DriverT>
  class ModuleAutoProbe final : public dev::Module {
    unsigned next_id = 0;
    ck::map<unsigned, ck::ref<DriverT>> m_drivers;

    Prober m_prober;

   public:
    ModuleAutoProbe(const char *name, Prober prober) : m_prober(prober) { set_name(name); }
    virtual void attach(ck::ref<hw::Device> dev) final {
      auto id = next_id++;

      auto drv = ck::make_ref<DriverT>(dev);
      drv->module = this;
      m_drivers.set(id, drv);
      drv->init();
    }

    virtual ProbeResult probe(ck::ref<hw::Device> dev) final { return m_prober(dev); }
  };



  class Driver : public ck::refcounted<Driver> {
   private:
    ck::ref<hw::Device> m_dev;

   public:
    dev::Module *module = nullptr;

    Driver(ck::ref<hw::Device> dev) : m_dev(dev) {
      // attach this driver to the device
      dev->attach_to(this);
      init();
    }
    virtual ~Driver(void) {}
    virtual void init(void) {}


    inline auto dev(void) const { return m_dev; }
  };


  class BlockDriver : public Driver {
   public:
		 using dev::Driver::Driver;
  };


}  // namespace dev


#define driver_init(name, T, prober)                                                                                \
  void _MOD_VARNAME(__driver_init)(void) { dev::Module::add(ck::make_ref<dev::ModuleAutoProbe<T>>(name, prober)); } \
  module_init(name, _MOD_VARNAME(__driver_init));
