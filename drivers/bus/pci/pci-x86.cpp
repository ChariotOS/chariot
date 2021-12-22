#include <asm.h>
#include <module.h>
#include <mem.h>
#include <pci.h>
#include <printk.h>

#include <dev/hardware.h>

#define PCI_VENDOR_ID 0x00            // word
#define PCI_DEVICE_ID 0x02            // word
#define PCI_COMMAND 0x04              // word
#define PCI_STATUS 0x06               // word
#define PCI_REVISION_ID 0x08          // byte
#define PCI_PROG_IF 0x09              // byte
#define PCI_SUBCLASS 0x0a             // byte
#define PCI_CLASS 0x0b                // byte
#define PCI_CACHE_LINE_SIZE 0x0c      // byte
#define PCI_LATENCY_TIMER 0x0d        // byte
#define PCI_HEADER_TYPE 0x0e          // byte
#define PCI_BIST 0x0f                 // byte
#define PCI_BAR0 0x10                 // uint32_t
#define PCI_BAR1 0x14                 // uint32_t
#define PCI_BAR2 0x18                 // uint32_t
#define PCI_BAR3 0x1C                 // uint32_t
#define PCI_BAR4 0x20                 // uint32_t
#define PCI_BAR5 0x24                 // uint32_t
#define PCI_SUBSYSTEM_ID 0x2C         // uint16_t
#define PCI_SUBSYSTEM_VENDOR_ID 0x2E  // uint16_t
#define PCI_INTERRUPT_LINE 0x3C       // byte
#define PCI_SECONDARY_BUS 0x19        // byte
#define PCI_HEADER_TYPE_DEVICE 0
#define PCI_HEADER_TYPE_BRIDGE 1
#define PCI_TYPE_BRIDGE 0x0604
#define PCI_ADDRESS_PORT 0xCF8
#define PCI_VALUE_PORT 0xCFC
#define PCI_NONE 0xFFFF

static uint32_t pci_cmd_port = 0xCF8;
static uint32_t pci_data_port = 0xCFC;



class X86PCIDevice : public hw::PCIDevice {
  pci::device *desc;

 public:
  X86PCIDevice(pci::device *d) : desc(d) {
    bus = d->bus;
    dev = d->dev;
    func = d->func;

    port_base = d->port_base;
    interrupt = d->interrupt;


    prog_if = pci::read(bus, dev, func, PCI_PROG_IF);

    vendor_id = d->vendor_id;
    device_id = d->device_id;

    class_id = d->class_id;
    subclass_id = d->subclass_id;
    interface_id = d->interface_id;
    revision = d->revision;
  }
  ~X86PCIDevice(void) override {}

  // ^hw::PCIDevice
  auto read8(uint32_t field) -> uint8_t override { return desc->read<uint8_t>(field); }
  auto read16(uint32_t field) -> uint16_t override { return desc->read<uint16_t>(field); }
  auto read32(uint32_t field) -> uint32_t override { return desc->read<uint32_t>(field); }
  auto read64(uint32_t field) -> uint64_t override { return desc->read<uint64_t>(field); }
  void write8(uint32_t field, uint8_t val) override { return desc->write<uint8_t>(field, val); }
  void write16(uint32_t field, uint16_t val) override { return desc->write<uint16_t>(field, val); }
  void write32(uint32_t field, uint32_t val) override { return desc->write<uint32_t>(field, val); }
  void write64(uint32_t field, uint64_t val) override { return desc->write<uint64_t>(field, val); }

  hw::PCIBar get_bar(int barnum) override {
    hw::PCIBar bar;
    bar.valid = false;
    bar.addr = NULL;

    auto headertype = read32(0x0E) & 0x7F;

    int max_bars = 6 - (4 * headertype);

    if (barnum >= max_bars) {
      return bar;
    }

    bar.valid = true;

    u64 bar_val = read32(0x10 + 4 * barnum);
    bar.raw = bar_val;

    bar.type = (bar_val & 0x1) ? hw::PCIBarType::BAR_PIO : hw::PCIBarType::BAR_MMIO;

    if (bar.type == hw::PCIBarType::BAR_MMIO) {
      switch ((bar_val >> 1) & 0x3) {
        case 0:  // 32 bit mode
        case 1:  // 20 bit mode
        case 3:  // 64 bit mode
          break;
      }

      bar.prefetchable = ((bar_val >> 3) & 0x1) == 0x1;
      bar.addr = (uint8_t *)(bar_val & ~0xF);
      // dunno
    } else {
      bar.addr = (uint8_t *)(bar_val & ~0x3);
      bar.prefetchable = false;
    }

    return bar;
  }
};



static inline uint32_t get_pci_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfun = (uint32_t)func;
  return 0x80000000u | (lbus << 16u) | (lslot << 11u) | (lfun << 8u) | (off & 0xfc);
}

uint32_t pci_cfg_readl(uint8_t bus, uint8_t slot, uint8_t fun, uint8_t off) {
  uint32_t addr;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfun = (uint32_t)fun;

  addr = (lbus << PCI_BUS_SHIFT) | (lslot << PCI_SLOT_SHIFT) | (lfun << PCI_FUN_SHIFT) | PCI_REG_MASK(off) | PCI_ENABLE_BIT;

  outl(PCI_CFG_ADDR_PORT, addr);
  return inl(PCI_CFG_DATA_PORT);
  return 0;
}

uint32_t pci::read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
  outl(PCI_CFG_ADDR_PORT, get_pci_addr(bus, slot, func, off));
  uint32_t ret = inl(PCI_CFG_DATA_PORT + off);
  return (ret >> ((off & 0x2) * 8));
  return 0;
}

void pci::write(uint8_t bus, uint16_t dev, uint16_t func, uint32_t off, uint32_t value) {
  // writing pci is simple. just calculate the id, then write that to the cmd
  // port, then write the data to the data port. simple
  outl(pci_cmd_port, get_pci_addr(bus, dev, func, off));
  outl(pci_data_port, value);
}

bool pci_device_has_functions(uint8_t bus, uint16_t dev) { return pci::read(bus, dev, 0x0, 0x0E) & (1 << 7); }

enum bar_type {
  bar_memory_mapping = 0,
  bar_in_out = 1,
};

struct pci_bar {
  bool prefetchable;
  uint8_t *addr;
  uint32_t size;
  enum bar_type type;
};

static struct pci_bar pci_get_bar(uint16_t bus, uint16_t dev, uint16_t func, uint32_t barnum) {
  struct pci_bar bar;
  bar.addr = nullptr;

  uint32_t headertype = pci::read(bus, dev, func, 0x0E) & 0x7F;

  uint32_t max_bars = 6 - (4 * headertype);

  if (barnum >= max_bars) {
    return bar;
  }

  u64 bar_val = pci::read(bus, dev, func, 0x10 + 4 * barnum);

  bar.type = (bar_val & 0x1) ? bar_in_out : bar_memory_mapping;

  if (bar.type == bar_memory_mapping) {
    switch ((bar_val >> 1) & 0x3) {
      case 0:  // 32 bit mode
      case 1:  // 20 bit mode
      case 3:  // 64 bit mode
        break;
    }

    bar.addr = (uint8_t *)(bar_val & ~0xF);
    bar.prefetchable = ((bar_val >> 3) & 0x1) == 0x1;
    // dunno
  } else {
    bar.addr = (uint8_t *)(bar_val & ~0x3);
    bar.prefetchable = false;
  }

  return bar;
}

bool read_device_descriptor(pci::device *desc, uint8_t bus, uint8_t dev, uint8_t func) {
  desc->bus = bus;
  desc->dev = dev;
  desc->func = func;

  desc->vendor_id = pci::read(bus, dev, func, PCI_VENDOR_ID);
  // bail if the device isn't valid
  if (desc->vendor_id == 0x0000 || desc->vendor_id == 0xFFFF) {
    return false;
  }

  desc->device_id = desc->read<uint16_t>(PCI_DEVICE_ID);

  auto class_code = desc->read<uint32_t>(0x08) >> 16;

  desc->class_id = (class_code >> 8) & 0xFF;
  desc->subclass_id = (class_code >> 0) & 0xFF;

  // /* TODO */
  // desc->interface_id = pci::read(bus, dev, func, 0x09);
  // desc->revision = pci::read(bus, dev, func, PCI_REVISION_ID);
  desc->interrupt = desc->read<uint8_t>(PCI_INTERRUPT_LINE);


  for (int i = 0; i < sizeof(desc->cfg); i += 4) {
    ((uint32_t *)(&desc->cfg))[i / 4] = desc->read<uint32_t>(i);
  }
  return true;
}

void pci_check_device(uint8_t bus, uint8_t device) {}

int pci_device_count = 0;
pci::device pci_devices[256];

static const char *pci_class_names[] = {
    [0x00] = "Unclassified",
    [0x01] = "Mass Storage Controller",
    [0x02] = "Network Controller",
    [0x03] = "Display Controller",
    [0x04] = "Multimedia Controller",
    [0x05] = "Memory Controller",
    [0x06] = "Bridge Device",
    [0x07] = "Simple Communication Controller",
    [0x08] = "Base System Peripheral",
    [0x09] = "Input Device Controller",
    [0x0A] = "Docking Station",
    [0x0B] = "Processor",
    [0x0C] = "Serial Bus Controller",
    [0x0D] = "Wireless Controller",
    [0x0E] = "Intelligent Controller",
    [0x0F] = "Satellite Communication Controller",
    [0x10] = "Encryption Controller",
    [0x11] = "Signal Processing Controller",
    [0x12] = "Processing Accelerator",
    [0x13] = "Non-Essential Instrumentation",
    [0x14] = "(Reserved)",
};

static void scan_bus(int bus, ck::ref<hw::MMIODevice> &root) {
  for (int dev = 0; dev < 32; dev++) {
    int nfuncs = 8; /* TODO: only scan functions if there are some */

    for (int func = 0; func < nfuncs; func++) {
      pci::device *desc = &pci_devices[pci_device_count];

      if (!read_device_descriptor(desc, bus, dev, func)) continue;

      const char *class_name = pci_class_names[desc->class_id];
      if (desc->class_id > 0x14) class_name = "Unknown";

      for (int barnum = 0; barnum < 6; barnum++) {
        struct pci_bar bar = pci_get_bar(bus, dev, func, barnum);
        if (bar.addr && (bar.type == bar_in_out)) {
          desc->port_base = (uint32_t)(u64)bar.addr;
        }
      }


      auto name = ck::string::format("%03x.%02x.%1x, %04x:%04x (%s)", bus, dev, func, desc->vendor_id, desc->device_id, class_name);
      auto dev = ck::make_ref<X86PCIDevice>(desc);
      dev->set_name(name);
      root->adopt(move(dev));
      pci_device_count++;
    }
  }
}

void pci::init(void) {
  auto root = ck::make_ref<hw::MMIODevice>(0, 0);
	root->compat().push("x86-pci-bus");


  auto start = arch_read_timestamp();
  // enumerate PCI devices
  auto header_type = (pci_cfg_readl(0, 0, 0, 0xc) >> 16) & 0xff;
  if ((header_type & 0x80) == 0) {
    /* Single PCI host controller */
    KINFO("Single PCI host controller (quick scan)\n");
    scan_bus(0, root);
  } else {
    KINFO("Multiple PCI host controllers (slower scan)\n");
    /* Multiple PCI host controllers */
    for (int bus = 0; bus < 32; bus++)
      scan_bus(bus, root);
  }
  KINFO("discovered %d PCI devices in %llu cycles.\n", pci_device_count, arch_read_timestamp() - start);

  hw::Device::add("x86-pci", root);
}

void pci::walk_devices(ck::func<void(device *)> fn) {
  for (uint32_t i = 0; i < pci_device_count; i++) {
    fn(&pci_devices[i]);
  }
}


pci::device *pci::find_generic_device(uint16_t class_id, uint16_t subclass_id) {
  for (uint32_t i = 0; i < pci_device_count; i++) {
    auto *dev = &pci_devices[i];
    if (dev->class_id == class_id && dev->subclass_id == subclass_id) {
      return dev;
    }
  }
  return NULL;
}

uint32_t pci::device::get_address(uint32_t off) { return get_pci_addr(bus, dev, func, off); }

bool pci::device::is_device(uint16_t v, uint16_t d) { return vendor_id == v && device_id == d; }

void pci::device::enable_bus_mastering(void) {
  // set bus mastering and enable io space
  adjust_ctrl_bits(PCI_CMD_BME | PCI_CMD_IOSE, 0);
}

void pci::device::adjust_ctrl_bits(int set, int clr) {
  uint16_t reg = read<uint16_t>(PCI_COMMAND);
  uint16_t new_reg = (reg | set) & ~clr;
  write<uint16_t>(PCI_COMMAND, new_reg);
}

hw::PCIBar pci::device::get_bar(int barnum) {
	hw::PCIBar bar;
  bar.valid = false;
  bar.addr = NULL;

  auto headertype = read<uint32_t>(0x0E) & 0x7F;

  int max_bars = 6 - (4 * headertype);

  if (barnum >= max_bars) {
    return bar;
  }

  bar.valid = true;

  u64 bar_val = read<uint32_t>(0x10 + 4 * barnum);
  bar.raw = bar_val;

  bar.type = (bar_val & 0x1) ? hw::PCIBarType::BAR_PIO : hw::PCIBarType::BAR_MMIO;

  if (bar.type == hw::PCIBarType::BAR_MMIO) {
    switch ((bar_val >> 1) & 0x3) {
      case 0:  // 32 bit mode
      case 1:  // 20 bit mode
      case 3:  // 64 bit mode
        break;
    }

    bar.prefetchable = ((bar_val >> 3) & 0x1) == 0x1;
    bar.addr = (uint8_t *)(bar_val & ~0xF);
    // dunno
  } else {
    bar.addr = (uint8_t *)(bar_val & ~0x3);
    bar.prefetchable = false;
  }

  return bar;
}
