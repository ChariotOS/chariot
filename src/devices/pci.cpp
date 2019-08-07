#include <mobo/dev_mgr.h>
#include <mobo/types.h>

#include <stdint.h>
#include <mutex>

#include <linux/serial_reg.h>
#include <linux/types.h>

template <typename T>
static inline T ioport_read(void *dst) {
  return *reinterpret_cast<T *>(dst);
}

template <typename T>
static inline void ioport_write(void *dst, T val) {
  *reinterpret_cast<T *>(dst) = val;
}

#define CONFIG_ADDR_PORT 0x0cf8
#define CONFIG_DATA_PORT 0x0cfc

#define PCI_DEV_IO_PORT_BASE 0xc000

#define PCI_SLOT_SHIFT 11
#define PCI_BUS_SHIFT 16
#define PCI_FUN_SHIFT 8

#define PCI_REG_MASK(x) ((x)&0xfc)
#define PCI_BUS_COUNT 1

class pci_bus : public mobo::device {
 private:
  struct pci_address {
    u8 bus;
    u8 slot;
    u8 func;
    u8 off;
  };

  struct pci_address parse_address(u32 addr) {
    struct pci_address p;

    p.slot = (addr >> PCI_SLOT_SHIFT) & 0xFF;
    p.bus = (addr >> PCI_BUS_SHIFT) & 0xFF;
    p.func = (addr >> PCI_FUN_SHIFT) & 0xFF;
    p.off = PCI_REG_MASK(addr);

    return p;
  }

 public:
  virtual std::vector<mobo::port_t> get_ports(void) {
    std::vector<mobo::port_t> ports;

    for (int i = 0; i < 4; i++) {
      ports.push_back(CONFIG_ADDR_PORT + i);
      ports.push_back(CONFIG_DATA_PORT + i);
    }
    return ports;
  }

  virtual int in(mobo::vcpu *, mobo::port_t port, int sz, void *data) {
    // printf("PCI IN: %04x\n", port);
    return 0;
  }

  virtual int out(mobo::vcpu *, mobo::port_t port, int sz, void *data) {
    if (port == CONFIG_ADDR_PORT) {
      // struct pci_address pa = parse_address(*(u32 *)data);
      // printf("PCI OUT: %02x %02x %02x %02x\n", pa.bus, pa.slot, pa.func, pa.off);
    }
    return 0;
  }
};

MOBO_REGISTER_DEVICE(pci_bus);
