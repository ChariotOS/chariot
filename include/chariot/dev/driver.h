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
    // private to the driver subsystem, do not fiddle with (write to) stuff below here
    rwlock lock;
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
  class Device;


  /*
   * A dev::Driver is an object that can be indexed by a device major number.
   * It is probed for devices it is interested in, then devices are attached
   * and converted to dev::Device instances if needed
   */
  class Driver : public ck::refcounted<Driver> {
    ck::string m_name;
    int m_major = -1;
    int m_next_minor = 0;

   protected:
    ck::map<unsigned, ck::ref<Device>> m_devices;

   public:
    Driver();
    virtual ~Driver(void) {}

    virtual ProbeResult probe(ck::ref<hw::Device> dev);

    // called by the hardware subsystem
    virtual void attach(ck::ref<hw::Device> dev) {}
    virtual void detach(ck::ref<hw::Device> dev) {}



    // the major number for this device driver.
    inline int major(void) const { return m_major; }
    inline void set_name(ck::string name) { m_name = move(name); }
    inline auto name(void) const { return m_name; }

    inline int next_minor(void) { return m_next_minor++; }

    static void add(ck::ref<dev::Driver>);
    static void probe_all(ck::ref<hw::Device>);
  };


  template <typename DeviceT>
  class ModuleDriver final : public dev::Driver {
    Prober m_prober;

   public:
    ModuleDriver(const char *name, Prober prober) : m_prober(prober) { set_name(name); }
    virtual void attach(ck::ref<hw::Device> dev) final {
      auto drv = ck::make_ref<DeviceT>(*this, dev);

      m_devices.set(drv->minor, drv);
      drv->init();
    }

    virtual ProbeResult probe(ck::ref<hw::Device> dev) final { return m_prober(dev); }
  };



  // This enum represents the type device that a dev::Device actually
  // is... This is required because chariot doesn't support rtti
  enum Type {
    Basic,   // dev::Device
    Char,    // dev::CharDevice
    Block,   // dev::BlockDevice
    Serial,  // dev::SerialDevice
  };

  /*
   * dev::Device objects represent the `minor` part of a `major/minor` device driver.
   * A Device object can be bound to the filesystem in /dev/, for example.
   */
  class Device : public ck::refcounted<Device> {
   private:
    // each driven device (minor number) must have a driver (major number),
    // this is a handle to that.
    dev::Driver &m_driver;
    // The hardware device being driven. This can be null for pseudo-devices
    ck::ref<hw::Device> m_dev = nullptr;


    dev::Type m_type = dev::Type::Basic;

   public:
    int major, minor;


    // Device drivers are not meant to reimplement the constructor.
    // They are meant to ovewrride `init()` instead
    Device(dev::Driver &driver, ck::ref<hw::Device> dev = nullptr) : m_driver(driver), m_dev(dev) {
      major = driver.major();
      minor = driver.next_minor();
      // attach this driver to the device
      if (dev) dev->attach_to(this);
      init();
    }
    virtual ~Device(void) {}
    // called once the device has been initialized
    virtual void init(void) {}
    // called after register_irq(...)
    virtual void irq(int num) {}

    inline auto dev(void) const { return m_dev; }
    inline auto driver(void) const { return m_driver; }


    void handle_irq(int num, const char *name);


    template <typename T>
    ck::ref<T> cast();
    auto device_type(void) const { return m_type; }

   protected:
    void set_type(dev::Type t) { m_type = t; }
  };


  template <dev::Type t>
  class TypedDevice : public dev::Device {
   public:
    static constexpr dev::Type TYPE = t;

    TypedDevice(dev::Driver &driver, ck::ref<hw::Device> dev = nullptr) : dev::Device(driver, dev) { set_type(t); }
    virtual ~TypedDevice(void) {}
  };



  class BlockDevice : public dev::TypedDevice<dev::Type::Block> {
   public:
    using dev::TypedDevice<dev::Type::Block>::TypedDevice;
    virtual ~BlockDevice() {}
  };



  class CharDevice : public dev::TypedDevice<dev::Type::Char> {
   public:
    using dev::TypedDevice<dev::Type::Char>::TypedDevice;
    virtual ~CharDevice() {}
  };



  class SerialDevice : public dev::TypedDevice<dev::Type::Serial> {
   public:
    using dev::TypedDevice<dev::Type::Serial>::TypedDevice;
    virtual ~SerialDevice() {}
  };



  template <typename T>
  inline auto dev::Device::cast() -> ck::ref<T> {
    if (device_type() == T::TYPE) return (T *)this;
    return nullptr;
  }

}  // namespace dev


#define driver_init(name, T, prober)                                                                             \
  void _MOD_VARNAME(__driver_init)(void) { dev::Driver::add(ck::make_ref<dev::ModuleDriver<T>>(name, prober)); } \
  module_init(name, _MOD_VARNAME(__driver_init));
