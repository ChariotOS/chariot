#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <ck/ptr.h>
#include <ck/string.h>
#include <types.h>

/**
 * a major number represents the device driver ID
 */
using major_t = u16;
using minor_t = u16;

struct dev_t {
  inline dev_t() : dev_t(0, 0) {}
  inline dev_t(major_t maj, minor_t min) : maj(maj), min(min) {}

  inline major_t major(void) { return maj; }
  inline minor_t minor(void) { return min; }

  inline bool operator==(const dev_t &o) { return o.maj == maj && o.min == min; }

 protected:
  major_t maj;
  minor_t min;
};

namespace dev {

  enum DeviceType { PCI, MMIO, Unknown };
  enum RemovalReason { HotPlug, Magic /* idk lol */ };

  // An abstract Device. You need to cast it to another kind
  // of device (PCIDevice / MMIODevice)
  class Device : public ck::refcounted<Device> {
    DeviceType m_type;
    ck::string m_name;
    bool m_active = false;

   public:
    Device(DeviceType t);

    static void add(ck::string name, ck::ref<Device>);
    static void remove(ck::ref<Device>, RemovalReason reason = HotPlug);
    static auto all(void) -> ck::vec<ck::ref<Device>>;

    // only registered devices can have names. Register it before calling this.
    ck::string name(void) const { return m_name; }
    virtual ~Device(void) {}

    template <typename T>
    ck::ref<T> cast();
    auto device_type(void) const { return m_type; }

   protected:
    void set_active(bool to) { m_active = to; }
    void set_name(ck::string name) { m_name = name; }
  };



  enum class PCIBarType : char {
    BAR_MMIO = 0,
    BAR_PIO = 1,
  };
  struct PCIBar {
    PCIBarType type;
    bool prefetchable;
    bool valid;
    uint8_t *addr;
    uint32_t size;
    uint32_t raw;
  };

  // A platform independent PCI device
  class PCIDevice : public dev::Device {
   public:
    uint16_t bus;
    uint16_t dev;
    uint16_t func;

    uint32_t port_base;
    uint8_t interrupt;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t interface_id;
    uint8_t revision;

    static constexpr DeviceType TYPE = DeviceType::PCI;
    PCIDevice() : dev::Device(TYPE) {}
    ~PCIDevice(void) override {}

    bool is_device(uint16_t vendor, uint16_t device) const;



    // Implement these if you implement a PCIDevice
    virtual auto get_bar(int barnum) -> dev::PCIBar { return {.valid = false}; };
    // read fields
    virtual auto read8(uint32_t field) -> uint8_t { return 0; }
    virtual auto read16(uint32_t field) -> uint16_t { return 0; }
    virtual auto read32(uint32_t field) -> uint32_t { return 0; }
    virtual auto read64(uint32_t field) -> uint64_t { return 0; }
    // write fields
    virtual auto write8(uint32_t field, uint8_t val) -> void {}
    virtual auto write16(uint32_t field, uint16_t val) -> void {}
    virtual auto write32(uint32_t field, uint32_t val) -> void {}
    virtual auto write64(uint32_t field, uint64_t val) -> void {}
  };


  // a device which is just a region and a length. By virtue
  // of being MMIO, any more information is implementation defined.
  class MMIODevice : public dev::Device {
   public:
    static constexpr DeviceType TYPE = DeviceType::MMIO;
    MMIODevice(off_t addr, size_t) : dev::Device(TYPE) {}
    ~MMIODevice(void) override {}
  };


  template <typename T>
  inline auto Device::cast() -> ck::ref<T> {
    if (device_type() == T::TYPE) return (T *)this;
    return nullptr;
  }

  // inline ck::ref<PCIDevice> Device::as_pci() { return is_mmio() ? (PCIDevice *)this : nullptr; }
  // inline ck::ref<MMIODevice> Device::as_mmio() { return is_mmio() ? (MMIODevice *)this : nullptr; }

};  // namespace dev

#endif
