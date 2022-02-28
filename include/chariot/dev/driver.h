#pragma once

#include <dev/hardware.h>
#include <fs.h>
#include <ck/ptr.h>
#include <ck/string.h>
#include <types.h>
#include <ck/map.h>

#define MAX_MAJOR 255


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
		int next_minor = 0;

   public:
    ModuleDriver(const char *name, Prober prober) : m_prober(prober) { set_name(name); }
    void attach(ck::ref<hw::Device> dev) override final {
      auto drv = ck::make_ref<DeviceT>(*this, dev);
			drv->minor = next_minor++;
      drv->init();
      m_devices.set(drv->minor, drv);
    }

    ProbeResult probe(ck::ref<hw::Device> dev) override final { return m_prober(dev); }
  };


  /*
   * dev::Device objects represent the `minor` part of a `major/minor` device driver.
   * A Device object can be bound to the filesystem in /dev/, for example.
   */
  class Device : public fs::Node {
   private:
    // each driven device (minor number) must have a driver (major number),
    // this is a handle to that.
    dev::Driver &m_driver;
    // The hardware device being driven. This can be null for pseudo-devices
    ck::ref<hw::Device> m_dev = nullptr;


   public:
    int major, minor;


    // Device drivers are not meant to reimplement the constructor.
    // They are meant to ovewrride `init()` instead
    Device(dev::Driver &driver, ck::ref<hw::Device> dev = nullptr);
    virtual ~Device(void);
    void bind(ck::string name);
    void unbind(void);
    inline auto dev(void) const { return m_dev; }
    inline auto &driver(void) const { return m_driver; }
    void handle_irq(int num, const char *name);

    // called once the device has been initialized
    virtual void init(void) {}
    // called after register_irq(...)
    virtual void irq(int num) {
			printk("irq handler!\n");
		}


    static scoped_irqlock lock_names(void);
    static ck::map<ck::string, ck::box<fs::DirectoryEntry>> &get_names(void);

    // ^fs::Node
    bool is_dev(void) override final { return true; }
  };


  class BlockDevice : public dev::Device {
   public:
    using dev::Device::Device;
    virtual ~BlockDevice() {}
    // ^fs::Node
    bool is_blockdev(void) override final { return true; }
    ssize_t read(fs::File &, char *dst, size_t count) override final;
    ssize_t write(fs::File &, const char *, size_t) override final;

    // nice wrappers for filesystems and all that :)
    inline int read_block(void *data, int block) { return read_blocks(block, data, 1); }
    inline int write_block(void *data, int block) { return write_blocks(block, data, 1); }

    virtual int read_blocks(uint32_t sector, void *data, int n) = 0;
    virtual int write_blocks(uint32_t sector, const void *data, int n) = 0;
  };



  class CharDevice : public dev::Device {
   public:
    using dev::Device::Device;

    virtual ~CharDevice() {}

    // ^dev::Device
    bool is_chardev(void) override final { return true; }
  };


}  // namespace dev


#define driver_init(name, T, prober)                                                                             \
  void _MOD_VARNAME(__driver_init)(void) { dev::Driver::add(ck::make_ref<dev::ModuleDriver<T>>(name, prober)); } \
  module_init(name, _MOD_VARNAME(__driver_init));


#define DECLARE_STUB_DRIVER(name, varname)                 \
  static class StubDriver_##varname : public dev::Driver { \
   public:                                                 \
    StubDriver_##varname(void) {                           \
      set_name(name);                                      \
      dev::Driver::add(this);                              \
      ref_retain(); /* hack */                             \
    }                                                      \
  } varname;
