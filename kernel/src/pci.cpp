#include <asm.h>
#include <pci.h>
#include <printk.h>
#include <module.h>

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

uint32_t pci_read(u8 bus, u8 slot, u8 func, u8 off) {
  u32 addr = get_pci_addr(bus, slot, func, off);
  outl(PCI_CFG_ADDR_PORT, addr);
  u32 ret = inl(PCI_CFG_DATA_PORT);
  return (ret >> ((off & 0x2) * 8)) & 0xffff;
}

void pci_write(u8 bus, u16 dev, u16 func, u32 off, u32 value) {
  // writing pci is simple. just calculate the id, then write that to the cmd
  // port, then write the data to the data port. simple
  outl(pci_cmd_port, get_pci_addr(bus, dev, func, off));
  outl(pci_data_port, value);
}

bool pci_device_has_functions(u8 bus, u16 dev) {
  return pci_read(bus, dev, 0x0, 0x0E) & (1 << 7);
}

struct pci_device_descriptor {
  bool valid;
  u32 port_base;
  u32 interrupt;
  u8 bus;
  u8 dev;
  u8 func;

  u16 vendor_id;
  u16 device_id;

  u8 class_id;
  u8 subclass_id;
  u8 interface_id;
  u8 revision;
};

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
  struct pci_bar bar = {
      .addr = NULL,
  };

  u32 headertype = pci_read(bus, dev, func, 0x0E) & 0x7F;

  int max_bars = 6 - (4 * headertype);

  if (barnum >= max_bars) {
    return bar;
  }

  u64 bar_val = pci_read(bus, dev, func, 0x10 + 4 * barnum);

  bar.type = (bar_val & 0x1) ? bar_in_out : bar_memory_mapping;

  if (bar.type == bar_memory_mapping) {
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

void read_device_descriptor(struct pci_device_descriptor *desc, u8 bus, u8 dev,
                            u8 func) {
  desc->bus = bus;
  desc->dev = dev;
  desc->func = func;

  desc->vendor_id = pci_read(bus, dev, func, 0x00);
  // bail if the device isn't valid
  if (desc->vendor_id == 0x0000 || desc->vendor_id == 0xFFFF) {
    desc->valid = false;
    return;
  }

  desc->valid = true;
  desc->device_id = pci_read(bus, dev, func, 0x02);

  desc->class_id = pci_read(bus, dev, func, 0x0B);
  desc->subclass_id = pci_read(bus, dev, func, 0x0A);
  desc->interface_id = pci_read(bus, dev, func, 0x09);

  desc->revision = pci_read(bus, dev, func, 0x08);
  desc->interrupt = pci_read(bus, dev, func, 0x3C);
}

void pci_check_device(u8 bus, u8 device) {}

int pci_device_count = 0;
struct pci_device_descriptor pci_devices[256];

int init_pci(void) {
  // enumerate PCI devices
  for (int bus = 0; bus < 32; bus++) {
    for (int dev = 0; dev < 32; dev++) {
      int nfuncs = pci_device_has_functions(bus, dev) ? 8 : 1;
      for (int func = 0; func < nfuncs; func++) {
        struct pci_device_descriptor *desc = &pci_devices[pci_device_count];

        read_device_descriptor(desc, bus, dev, func);
        if (!desc->valid) continue;

        /*
        printk("%02x.%02x.%1x: %04x:%04x\n", bus, dev, func, desc->vendor_id,
               desc->device_id);
               */

        for (int barnum = 0; barnum < 6; barnum++) {
          struct pci_bar bar = pci_get_bar(bus, dev, func, barnum);
          if (bar.addr && (bar.type == bar_in_out)) {
            desc->port_base = (u32)bar.addr;
          }
        }

        pci_device_count++;
      }
    }
  }

  printk("discovered %d PCI devices.\n", pci_device_count);
  return 0;
}

static void init_test(void) {
  printk("init_test\n");
}

module_init("init", init_test);
