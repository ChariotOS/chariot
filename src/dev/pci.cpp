#include <asm.h>
#include <module.h>
#include <pci.h>
#include <printk.h>

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
#define PCI_BAR0 0x10                 // u32
#define PCI_BAR1 0x14                 // u32
#define PCI_BAR2 0x18                 // u32
#define PCI_BAR3 0x1C                 // u32
#define PCI_BAR4 0x20                 // u32
#define PCI_BAR5 0x24                 // u32
#define PCI_SUBSYSTEM_ID 0x2C         // u16
#define PCI_SUBSYSTEM_VENDOR_ID 0x2E  // u16
#define PCI_INTERRUPT_LINE 0x3C       // byte
#define PCI_SECONDARY_BUS 0x19        // byte
#define PCI_HEADER_TYPE_DEVICE 0
#define PCI_HEADER_TYPE_BRIDGE 1
#define PCI_TYPE_BRIDGE 0x0604
#define PCI_ADDRESS_PORT 0xCF8
#define PCI_VALUE_PORT 0xCFC
#define PCI_NONE 0xFFFF

static u32 pci_cmd_port = 0xCF8;
static u32 pci_data_port = 0xCFC;

static inline u32 get_pci_addr(u8 bus, u8 slot, u8 func, u8 off) {
  u32 addr;
  u32 lbus = (uint32_t)bus;
  u32 lslot = (uint32_t)slot;
  u32 lfun = (uint32_t)func;

  addr = (lbus << PCI_BUS_SHIFT) | (lslot << PCI_SLOT_SHIFT) |
         (lfun << PCI_FUN_SHIFT) | (off & 0xfc) | PCI_ENABLE_BIT;
  return addr;
}

uint32_t pci::read(u8 bus, u8 slot, u8 func, u8 off) {
  u32 addr = get_pci_addr(bus, slot, func, off);
  outl(PCI_CFG_ADDR_PORT, addr);
  u32 ret = inl(PCI_CFG_DATA_PORT);
  return (ret >> ((off & 0x2) * 8)) & 0xffff;
}

void pci::write(u8 bus, u16 dev, u16 func, u32 off, u32 value) {
  // writing pci is simple. just calculate the id, then write that to the cmd
  // port, then write the data to the data port. simple
  outl(pci_cmd_port, get_pci_addr(bus, dev, func, off));
  outl(pci_data_port, value);
}

bool pci_device_has_functions(u8 bus, u16 dev) {
  return pci::read(bus, dev, 0x0, 0x0E) & (1 << 7);
}

enum bar_type {
  bar_memory_mapping = 0,
  bar_in_out = 1,
};

struct pci_bar {
  bool prefetchable;
  u8 *addr;
  u32 size;
  enum bar_type type;
};

static struct pci_bar pci_get_bar(u16 bus, u16 dev, u16 func, u32 barnum) {
  struct pci_bar bar;
  bar.addr = nullptr;

  u32 headertype = pci::read(bus, dev, func, 0x0E) & 0x7F;

  u32 max_bars = 6 - (4 * headertype);

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

    bar.addr = (u8 *)(bar_val & ~0xF);
    bar.prefetchable = ((bar_val >> 3) & 0x1) == 0x1;
    // dunno
  } else {
    bar.addr = (u8 *)(bar_val & ~0x3);
    bar.prefetchable = false;
  }

  return bar;
}

bool read_device_descriptor(pci::device *desc, u8 bus, u8 dev, u8 func) {
  desc->bus = bus;
  desc->dev = dev;
  desc->func = func;

  desc->vendor_id = pci::read(bus, dev, func, 0x00);
  // bail if the device isn't valid
  if (desc->vendor_id == 0x0000 || desc->vendor_id == 0xFFFF) {
    return false;
  }

  desc->device_id = pci::read(bus, dev, func, 0x02);

  desc->class_id = pci::read(bus, dev, func, 0x0B);
  desc->subclass_id = pci::read(bus, dev, func, 0x0A);
  desc->interface_id = pci::read(bus, dev, func, 0x09);

  desc->revision = pci::read(bus, dev, func, 0x08);
  desc->interrupt = pci::read(bus, dev, func, 0x3C);
  return true;
}

void pci_check_device(u8 bus, u8 device) {}

int pci_device_count = 0;
pci::device pci_devices[256];

void pci::init(void) {
  // enumerate PCI devices
  for (int bus = 0; bus < 32; bus++) {
    for (int dev = 0; dev < 32; dev++) {
      int nfuncs = pci_device_has_functions(bus, dev) ? 8 : 1;
      for (int func = 0; func < nfuncs; func++) {
        pci::device *desc = &pci_devices[pci_device_count];

        if (!read_device_descriptor(desc, bus, dev, func)) continue;

        KINFO("pci device: %03x.%02x.%1x: %04x:%04x\n", bus, dev, func,
              desc->vendor_id, desc->device_id);

        for (int barnum = 0; barnum < 6; barnum++) {
          struct pci_bar bar = pci_get_bar(bus, dev, func, barnum);
          if (bar.addr && (bar.type == bar_in_out)) {
            desc->port_base = (u32)(u64)bar.addr;
          }
        }

        pci_device_count++;
      }
    }
  }

  KINFO("discovered %d PCI devices.\n", pci_device_count);
}

void pci::walk_devices(func<void(device *)> fn) {
  for (u32 i = 0; i < pci_device_count; i++) {
    fn(&pci_devices[i]);
  }
}

u32 pci::device::get_address(u32 off) {
  return get_pci_addr(bus, dev, func, off);
}

bool pci::device::is_device(u16 v, u16 d) {
  return vendor_id == v && device_id == d;
}

void pci::device::enable_bus_mastering(void) {
  auto value = read<u16>(PCI_COMMAND);
  value |= (1 << 2);
  value |= (1 << 0);
  write<u16>(PCI_COMMAND, value);
}

pci::bar pci::device::get_bar(int barnum) {
  pci::bar bar;
  bar.valid = false;
  bar.addr = NULL;

  auto headertype = read<u32>(0x0E) & 0x7F;

  int max_bars = 6 - (4 * headertype);

  if (barnum >= max_bars) {
    return bar;
  }

  bar.valid = true;

  u64 bar_val = read<u32>(0x10 + 4 * barnum);
  bar.raw = bar_val;

  bar.type = (bar_val & 0x1) ? bar_type::BAR_PIO : bar_type::BAR_MMIO;

  if (bar.type == bar_type::BAR_MMIO) {
    switch ((bar_val >> 1) & 0x3) {
      case 0:  // 32 bit mode
      case 1:  // 20 bit mode
      case 3:  // 64 bit mode
        break;
    }

    bar.prefetchable = ((bar_val >> 3) & 0x1) == 0x1;
    // dunno
  } else {
    bar.addr = (u8 *)(bar_val & ~0x3);
    bar.prefetchable = false;
  }

  return bar;
}

static void placeholder(void) {}
module_init("placeholder", placeholder);
