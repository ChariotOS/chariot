#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <ck/ptr.h>
#include <ck/string.h>
#include <ck/option.h>
#include <ck/map.h>
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

  class Device;
};

namespace hw {

  enum DeviceType { PCI, MMIO, Unknown };
  enum RemovalReason { HotPlug, Magic /* idk lol */ };




  // to represent device tree registers
  struct Reg {
    uint64_t address, size;
  };

  struct Prop {
    enum Type { Unknown, String, Integer, Register, List, Interrupts, CellSize };
    Type type;
    ck::vec<uint8_t> data;
    // size of the cells from the device tree
    uint8_t size_cells = 1;
    uint8_t address_cells = 1;
    uint8_t irq_cells = 1;

    ck::string format(void) const;

    // read an address or a size (from the devicetree size/address cell sizes)
    bool read_address(uint64_t *dst, off_t &off) const { return read_cell(dst, address_cells, off); }
    bool read_size(uint64_t *dst, off_t &off) const { return read_cell(dst, size_cells, off); }
    bool read_irq(uint64_t *dst, off_t &off) const { return read_cell(dst, irq_cells, off); }

    // read integers using a format string. s = size, a = addr, i = irq
    // for example, reading a 'reg' type from the device tree uses the format "as"
    bool read_ints(ck::vec<uint64_t> &dst, const char *fmt, off_t &off) const;
    bool read_all_ints(ck::vec<uint64_t> &dst, const char *fmt) const;

    bool read_cell(uint64_t *dst, int cell_size, off_t &off) const;


    ck::vec<hw::Reg> read_registers(void) const;
  };

  // An abstract Device. You need to cast it to another kind
  // of device (PCIDevice / MMIODevice)
  class Device : public ck::refcounted<Device> {
    DeviceType m_type;
    ck::string m_name;
    bool m_active = false;
    ck::ref<hw::Device> m_parent = nullptr;
    dev::Device *m_device = nullptr;
    ck::vec<ck::ref<hw::Device>> m_children;

   protected:
    // when added to the main device layer, a device is locked.
    // Children cannot be added anymore.
    bool m_locked = false;
    ck::map<ck::string, Prop> m_props;

   public:
    Device(DeviceType t);

    static void add(ck::string name, ck::ref<Device>);
    static void remove(ck::ref<Device>, RemovalReason reason = HotPlug);
    static auto all(void) -> ck::vec<ck::ref<Device>>;


    // lock the device. No more changes can occur (adopt, prop add, etc);
    inline void lock(void) { m_locked = true; }


    void attach_to(dev::Device *device);
    void detach(void) { m_device = nullptr; }
    dev::Device *attached_device(void) const { return m_device; }

    inline auto &children(void) const { return m_children; }
    inline auto parent(void) const { return m_parent; }
    inline void adopt(ck::ref<hw::Device> child) {
      assert(!m_locked);
      child->m_parent = this;
      m_children.push(child);
    }


    inline auto &props(void) { return m_props; }
    void add_property(ck::string name, Prop &&prop);
    ck::option<ck::string> get_prop_string(const ck::string &name);
    ck::option<uint64_t> get_prop_int(const ck::string &name);

    // only registered devices can have names. Register it before calling this.
    ck::string name(void) const { return m_name; }
    void set_name(ck::string name) { m_name = name; }
    virtual ~Device(void) {}

    template <typename T>
    ck::ref<T> cast();
    auto device_type(void) const { return m_type; }

   protected:
    void set_active(bool to) { m_active = to; }
  };



	ck::ref<hw::Device> find_phandle(int phandle);

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
  class PCIDevice : public hw::Device {
   public:
    uint16_t bus;
    uint16_t dev;
    uint16_t func;

    uint32_t port_base;
    uint8_t interrupt;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t prog_if;  // programming interface
    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t interface_id;
    uint8_t revision;

    static constexpr DeviceType TYPE = DeviceType::PCI;
    PCIDevice() : hw::Device(TYPE) {}
    ~PCIDevice(void) override {}

    bool is_device(uint16_t vendor, uint16_t device) const;



    // Implement these if you implement a PCIDevice
    virtual auto get_bar(int barnum) -> hw::PCIBar { return {.valid = false}; };
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
  class MMIODevice : public hw::Device {
   public:
    static constexpr DeviceType TYPE = DeviceType::MMIO;
    MMIODevice(off_t addr, size_t) : hw::Device(TYPE) { m_address = addr; }
    ~MMIODevice(void) override {}


    inline auto &compat() { return m_compat; }

    bool is_compat(const char *driver) const {
      for (auto &compat : m_compat) {
        if (compat == driver) return true;
      }
      return false;
    }

    off_t address(void) const { return m_address; }

    // if this is -1, we dont know what the interrupt is.
    int interrupt = -1;

   private:
    off_t m_address;
    ck::vec<ck::string> m_compat;
  };


  template <typename T>
  inline auto Device::cast() -> ck::ref<T> {
    if (device_type() == T::TYPE) return (T *)this;
    return nullptr;
  }

};  // namespace hw

#endif
