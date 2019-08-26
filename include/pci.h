
/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the
 * United States National  Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu>
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifndef __PCI_H__
#define __PCI_H__

#include <asm.h>
#include <func.h>
#include <types.h>

#define __packed __attribute__((packed))
// #include <nautilus/list.h>

#define PCI_CFG_ADDR_PORT 0xcf8
#define PCI_CFG_DATA_PORT 0xcfc

#define PCI_SLOT_SHIFT 11
#define PCI_BUS_SHIFT 16
#define PCI_FUN_SHIFT 8

#define PCI_REG_MASK(x) ((x)&0xfc)

#define PCI_ENABLE_BIT 0x80000000UL

#define PCI_MAX_BUS 256
#define PCI_MAX_DEV 32
#define PCI_MAX_FUN 8

#define PCI_CLASS_LEGACY 0x0
#define PCI_CLASS_STORAGE 0x1
#define PCI_CLASS_NET 0x2
#define PCI_CLASS_DISPLAY 0x3
#define PCI_CLASS_MULTIM 0x4
#define PCI_CLASS_MEM 0x5
#define PCI_CLASS_BRIDGE 0x6
#define PCI_CLASS_SIMPLE 0x7
#define PCI_CLASS_BSP 0x8
#define PCI_CLASS_INPUT 0x9
#define PCI_CLASS_DOCK 0xa
#define PCI_CLASS_PROC 0xb
#define PCI_CLASS_SERIAL 0xc
#define PCI_CLASS_WIRELESS 0xd
#define PCI_CLASS_INTIO 0xe
#define PCI_CLASS_SAT 0xf
#define PCI_CLASS_CRYPTO 0x10
#define PCI_CLASS_SIG 0x11
#define PCI_CLASS_NOCLASS 0xff

#define PCI_SUBCLASS_BRIDGE_PCI 0x4
#define PCI_BAR_OFFSET 0x10

#define PCI_BAR_UAR_OFFSET 0x18

#define PCI_BAR_SIZE_MAGIC 0xffffffff
#define PCI_BAR_IO_MASK 0xfffffffc
#define PCI_BAR_MEM_MASK 0xfffffff0
#define PCI_BAR_MEM_MASK64 0xfffffffffffffff0

#define PCI_GET_MBAR_TYPE(b) (((b) >> 1) & 0x3)

#define PCI_MBAR_TYPE_32 0x0
#define PCI_MBAR_TYPE_RSVD0 0x1
#define PCI_MBAR_TYPE_64 0x2
#define PCI_MBAR_TYPE_RSVD1 0x3

#define PCI_MBAR_IS_64(b) (PCI_GET_MBAR_TYPE(b) == PCI_MBAR_TYPE_64)
#define PCI_MBAR_IS_32(b) (PCI_GET_MBAR_TYPE(b) == PCI_MBAR_TYPE_32)

struct naut_info;

struct pci_bus {
  u32 num;
  // struct list_head bus_node;
  // struct list_head dev_list;
  struct pci_info *pci;
};

struct pci_cfg_space {
  u16 vendor_id;
  u16 device_id;
  u16 cmd;
  u16 status;
  u8 rev_id;
  u8 prog_if;
  u8 subclass;
  u8 class_code;
  u8 cl_size;
  u8 lat_timer;
  u8 hdr_type;
  u8 bist;

  union {
    // type = 00h (device)
    struct {
      uint32_t bars[6];
      uint32_t cardbus_cis_ptr;
      uint16_t subsys_vendor_id;
      uint16_t subsys_id;
      uint32_t exp_rom_bar;
      uint8_t cap_ptr;
      uint8_t rsvd[7];
      uint8_t intr_line;
      uint8_t intr_pin;
      uint8_t min_grant;
      uint8_t max_latency;
      uint32_t data[48];
    } __packed dev_cfg;

    // type = 01h (PCI-to-PCI bridge)
    struct {
      uint32_t bars[2];
      uint8_t primary_bus_num;
      uint8_t secondary_bus_num;
      uint8_t sub_bus_num;
      uint8_t secondary_lat_timer;
      uint8_t io_base;
      uint8_t io_limit;
      uint16_t secondary_status;
      uint16_t mem_base;
      uint16_t mem_limit;
      uint16_t prefetch_mem_base;
      uint16_t prefetch_mem_limit;
      uint32_t prefetch_base_upper;
      uint32_t prefetch_limit_upper;
      uint16_t io_base_upper;
      uint16_t io_limit_upper;
      uint8_t cap_ptr;
      uint8_t rsvd[3];
      uint32_t exp_rom_bar;
      uint8_t intr_line;
      uint8_t intr_pin;
      uint16_t bridge_ctrl;
      uint32_t data[48];
    } __packed pci_to_pci_bridge_cfg;

    struct {
      uint32_t cardbus_socket_bar;
      uint8_t cap_list_offset;
      uint8_t rsvd;
      uint16_t secondary_status;
      uint8_t pci_bus_num;
      uint8_t cardbus_bus_num;
      uint8_t sub_bus_num;
      uint8_t cardbus_lat_timer;
      uint32_t mem_base0;
      uint32_t mem_limit0;
      uint32_t mem_base1;
      uint32_t mem_limit1;
      uint32_t io_base0;
      uint32_t io_limit0;
      uint32_t io_base1;
      uint32_t io_limit1;
      uint8_t intr_line;
      uint8_t intr_pin;
      uint16_t bridge_ctrl;
      uint16_t subsys_dev_id;
      uint16_t subsys_vendor_id;
      uint32_t legacy_base;
      uint32_t rsvd1[14];
      uint32_t data[32];
    } __packed cardbus_bridge_cfg;

  } __packed;

} __packed;

typedef enum { PCI_BAR_NONE = 0, PCI_BAR_MEM, PCI_BAR_IO } pci_bar_type_t;

typedef enum {
  PCI_MSI_NONE = 0,
  PCI_MSI_32,
  PCI_MSI_64,
  PCI_MSI_32_PER_VEC,
  PCI_MSI_64_PER_VEC
} pci_msi_type_t;

struct pci_msi_info {
  int enabled;
  pci_msi_type_t type;
  uint8_t co;  // offset of capability in the config space

  // these come from a query of the device
  int num_vectors_needed;

  // these come from the user -  these reflect how the
  // user configured the device
  int base_vec;  // interrupt will occur on [vec,vec+num_vecs_used)
  int num_vecs;
  int target_cpu;
};

typedef enum { PCI_MSI_X_NONE = 0, PCI_MSI_X } pci_msi_x_type_t;

typedef struct pci_msi_x_table_entry {
  uint32_t msg_addr_lo;
  uint32_t msg_addr_hi;
  uint32_t msg_data;
  uint32_t vector_control;
} __packed __attribute__((aligned(4))) pci_msi_x_table_entry_t;

struct pci_msi_x_info {
  int enabled;
  pci_msi_x_type_t type;
  uint8_t co;  // offset of capability in the config space

  // these come from a query of the device at boot
  uint32_t size;
  pci_msi_x_table_entry_t *table;  // points to MSI-X table on device
  uint64_t *pending;               // points to pending bits array on device
};

struct pci_dev {
  uint32_t num;
  uint32_t fun;
  struct pci_bus *bus;
  // struct list_head dev_node;
  struct pci_cfg_space cfg;  // only a snapshot at boot!
  struct pci_msi_info msi;
  struct pci_msi_x_info msix;
};

struct pci_info {
  uint32_t num_buses;
  // struct list_head bus_list;
};

namespace pci {

enum class bar_type : char {
  BAR_MMIO = 0,
  BAR_PIO = 1,
};
struct bar {
  bar_type type;
  bool prefetchable;
  bool valid;
  u8 *addr;
  u32 size;
  u32 raw;
};

class device {
 public:
  bool valid = false;
  u16 bus, dev, func;

  u32 port_base;
  u32 interrupt;

  u16 vendor_id;
  u16 device_id;

  u8 class_id;
  u8 subclass_id;
  u8 interface_id;
  u8 revision;

  u32 get_address(u32);

  pci::bar get_bar(int barnum);

  bool is_device(u16 vendor, u16 device);

  template <typename T>
  T read(u32 field) {
    outl(PCI_CFG_ADDR_PORT, get_address(field));
    if constexpr (sizeof(T) == 4) return inl(PCI_CFG_DATA_PORT);
    if constexpr (sizeof(T) == 2) return inw(PCI_CFG_DATA_PORT + (field & 2));
    if constexpr (sizeof(T) == 1) return inb(PCI_CFG_DATA_PORT + (field & 3));

    panic("invalid PCI read of size %d\n", sizeof(T));
  }

  template <typename T>
  void write(u32 field, T val) {
    outl(PCI_CFG_ADDR_PORT, get_address(field));
    if constexpr (sizeof(T) == 4) return outl(PCI_CFG_DATA_PORT, val);
    if constexpr (sizeof(T) == 2)
      return outw(PCI_CFG_DATA_PORT + (field & 2), val);
    if constexpr (sizeof(T) == 1)
      return outb(PCI_CFG_DATA_PORT + (field & 3), val);

    panic("invalid PCI write of size %d\n", sizeof(T));
  }
  void enable_bus_mastering(void);
};

void init();

u32 read(u8 bus, u8 dev, u8 func, u8 off);
void write(u8 bus, u16 dev, u16 func, u32 reg_off, u32 value);

void walk_devices(func<void(device *)>);

};  // namespace pci

#endif
