#ifndef __PCI_H__
#define __PCI_H__

#include <asm.h>
#include <ck/func.h>
#include <types.h>

#include <dev/hardware.h>

#define __packed __attribute__((packed))

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


#define PCI_SUBCLASS_IDE 0x1
#define PCI_SUBCLASS_FLOPPY 0x2
#define PCI_SUBCLASS_ATA 0x5
#define PCI_SUBCLASS_SATA 0x6
#define PCI_SUBCLASS_NVME 0x8

#define PCI_SUBCLASS_ETHERNET 0x0

#define PCI_SUBCLASS_VGA_COMPATIBLE 0x0
#define PCI_SUBCLASS_XGA 0x1

#define PCI_SUBCLASS_USB 0x3

#define PCI_PROGIF_UHCI 0x20
#define PCI_PROGIF_OHCI 0x10
#define PCI_PROGIF_EHCI 0x20
#define PCI_PROGIF_XHCI 0x30

#define PCI_IO_PORT_CONFIG_ADDRESS 0xCF8
#define PCI_IO_PORT_CONFIG_DATA 0xCFC

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



#define PCI_CMD 0x04

#define PCI_CMD_ID_BIT 10
#define PCI_CMD_FBE_BIT 9
#define PCI_CMD_SEE_BIT 8
#define PCI_CMD_PEE_BIT 6
#define PCI_CMD_VGA_BIT 5
#define PCI_CMD_MWIE_BIT 4
#define PCI_CMD_SCE_BIT 3
#define PCI_CMD_BME_BIT 2
#define PCI_CMD_MSE_BIT 1
#define PCI_CMD_IOSE_BIT 0

#define PCI_CMD_ID_BITS 1
#define PCI_CMD_FBE_BITS 1
#define PCI_CMD_SEE_BITS 1
#define PCI_CMD_PEE_BITS 1
#define PCI_CMD_VGA_BITS 1
#define PCI_CMD_MWIE_BITS 1
#define PCI_CMD_SCE_BITS 1
#define PCI_CMD_BME_BITS 1
#define PCI_CMD_MSE_BITS 1
#define PCI_CMD_IOSE_BITS 1
#define PCI_CMD_ID_MASK ((1U << PCI_CMD_ID_BITS) - 1)
#define PCI_CMD_FBE_MASK ((1U << PCI_CMD_FBE_BITS) - 1)
#define PCI_CMD_SEE_MASK ((1U << PCI_CMD_SEE_BITS) - 1)
#define PCI_CMD_PEE_MASK ((1U << PCI_CMD_PEE_BITS) - 1)
#define PCI_CMD_VGA_MASK ((1U << PCI_CMD_VGA_BITS) - 1)
#define PCI_CMD_MWIE_MASK ((1U << PCI_CMD_MWIE_BITS) - 1)
#define PCI_CMD_SCE_MASK ((1U << PCI_CMD_SCE_BITS) - 1)
#define PCI_CMD_BME_MASK ((1U << PCI_CMD_BME_BITS) - 1)
#define PCI_CMD_MSE_MASK ((1U << PCI_CMD_MSE_BITS) - 1)
#define PCI_CMD_IOSE_MASK ((1U << PCI_CMD_IOSE_BITS) - 1)

// Interrupt pin disable (does not affect MSI)
#define PCI_CMD_ID (PCI_CMD_ID_MASK << PCI_CMD_ID_BIT)

// Fast back-to-back enable
#define PCI_CMD_FBE (PCI_CMD_FBE_MASK << PCI_CMD_FBE_BIT)

// SERR# Enable
#define PCI_CMD_SEE (PCI_CMD_SEE_MASK << PCI_CMD_SEE_BIT)

// Parity error response enable
#define PCI_CMD_PEE (PCI_CMD_PEE_MASK << PCI_CMD_PEE_BIT)

// VGA palette snooping enable
#define PCI_CMD_VGA (PCI_CMD_VGA_MASK << PCI_CMD_VGA_BIT)

// Memory write and invalidate enable
#define PCI_CMD_MWIE (PCI_CMD_MWIE_MASK << PCI_CMD_MWIE_BIT)

// Special cycle enable
#define PCI_CMD_SCE (PCI_CMD_SCE_MASK << PCI_CMD_SCE_BIT)

// Bus master enable
#define PCI_CMD_BME (PCI_CMD_BME_MASK << PCI_CMD_BME_BIT)

// Memory space enable
#define PCI_CMD_MSE (PCI_CMD_MSE_MASK << PCI_CMD_MSE_BIT)

// I/O space enable
#define PCI_CMD_IOSE (PCI_CMD_IOSE_MASK << PCI_CMD_IOSE_BIT)

#define PCI_CMD_ID_n(n) ((n) << PCI_CMD_ID_BIT)
#define PCI_CMD_FBE_n(n) ((n) << PCI_CMD_FBE_BIT)
#define PCI_CMD_SEE_n(n) ((n) << PCI_CMD_SEE_BIT)
#define PCI_CMD_PEE_n(n) ((n) << PCI_CMD_PEE_BIT)
#define PCI_CMD_VGA_n(n) ((n) << PCI_CMD_VGA_BIT)
#define PCI_CMD_MWIE_n(n) ((n) << PCI_CMD_MWIE_BIT)
#define PCI_CMD_SCE_n(n) ((n) << PCI_CMD_SCE_BIT)
#define PCI_CMD_BME_n(n) ((n) << PCI_CMD_BME_BIT)
#define PCI_CMD_MSE_n(n) ((n) << PCI_CMD_MSE_BIT)
#define PCI_CMD_IOSE_n(n) ((n) << PCI_CMD_IOSE_BIT)

#define PCI_CMD_ID_GET(n) (((n) >> PCI_CMD_ID_BIT) & PCI_CMD_ID_MASK)
#define PCI_CMD_FBE_GET(n) (((n) >> PCI_CMD_FBE_BIT) & PCI_CMD_FBE_MASK)
#define PCI_CMD_SEE_GET(n) (((n) >> PCI_CMD_SEE_BIT) & PCI_CMD_SEE_MASK)
#define PCI_CMD_PEE_GET(n) (((n) >> PCI_CMD_PEE_BIT) & PCI_CMD_PEE_MASK)
#define PCI_CMD_VGA_GET(n) (((n) >> PCI_CMD_VGA_BIT) & PCI_CMD_VGA_MASK)
#define PCI_CMD_MWIE_GET(n) (((n) >> PCI_CMD_MWIE_BIT) & PCI_CMD_MWIE_MASK)
#define PCI_CMD_SCE_GET(n) (((n) >> PCI_CMD_SCE_BIT) & PCI_CMD_SCE_MASK)
#define PCI_CMD_BME_GET(n) (((n) >> PCI_CMD_BME_BIT) & PCI_CMD_BME_MASK)
#define PCI_CMD_MSE_GET(n) (((n) >> PCI_CMD_MSE_BIT) & PCI_CMD_MSE_MASK)
#define PCI_CMD_IOSE_GET(n) (((n) >> PCI_CMD_IOSE_BIT) & PCI_CMD_IOSE_MASK)

#define PCI_CMD_ID_SET(r, n) ((r) = ((r) & ~PCI_CMD_ID) | PCI_CMD_ID_n((n)))
#define PCI_CMD_FBE_SET(r, n) ((r) = ((r) & ~PCI_CMD_FBE) | PCI_CMD_FBE_n((n)))
#define PCI_CMD_SEE_SET(r, n) ((r) = ((r) & ~PCI_CMD_SEE) | PCI_CMD_SEE_n((n)))
#define PCI_CMD_PEE_SET(r, n) ((r) = ((r) & ~PCI_CMD_PEE) | PCI_CMD_PEE_n((n)))
#define PCI_CMD_VGA_SET(r, n) ((r) = ((r) & ~PCI_CMD_VGA) | PCI_CMD_VGA_n((n)))
#define PCI_CMD_MWIE_SET(r, n) ((r) = ((r) & ~PCI_CMD_MWIE) | PCI_CMD_MWIE_n((n)))
#define PCI_CMD_SCE_SET(r, n) ((r) = ((r) & ~PCI_CMD_SCE) | PCI_CMD_SCE_n((n)))
#define PCI_CMD_BME_SET(r, n) ((r) = ((r) & ~PCI_CMD_BME) | PCI_CMD_BME_n((n)))
#define PCI_CMD_MSE_SET(r, n) ((r) = ((r) & ~PCI_CMD_MSE) | PCI_CMD_MSE_n((n)))
#define PCI_CMD_IOSE_SET(r, n) ((r) = ((r) & ~PCI_CMD_IOSE) | PCI_CMD_IOSE_n((n)))

struct pci_bus {
  uint32_t num;
  // struct list_head bus_node;
  // struct list_head dev_list;
  struct pci_info *pci;
};

struct pci_cfg_space {
  uint16_t vendor_id;
  uint16_t device_id;
  uint16_t cmd;
  uint16_t status;
  uint8_t rev_id;
  uint8_t prog_if;
  uint8_t subclass;
  uint8_t class_code;
  uint8_t cl_size;
  uint8_t lat_timer;
  uint8_t hdr_type;
  uint8_t bist;

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

typedef enum { PCI_MSI_NONE = 0, PCI_MSI_32, PCI_MSI_64, PCI_MSI_32_PER_VEC, PCI_MSI_64_PER_VEC } pci_msi_type_t;

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


  class device {
   public:
    bool valid = false;
    uint16_t bus, dev, func;

    uint32_t port_base;
    uint8_t interrupt;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t interface_id;
    uint8_t revision;

    uint32_t get_address(uint32_t);


    struct pci_cfg_space cfg;  // snapshot at boot!

    hw::PCIBar get_bar(int barnum);

    bool is_device(uint16_t vendor, uint16_t device);

    template <typename T>
    T read(uint32_t field) {
      outl(PCI_CFG_ADDR_PORT, get_address(field));
      if constexpr (sizeof(T) == 4) return inl(PCI_CFG_DATA_PORT);
      if constexpr (sizeof(T) == 2) return inw(PCI_CFG_DATA_PORT + (field & 2));
      if constexpr (sizeof(T) == 1) return inb(PCI_CFG_DATA_PORT + (field & 3));
      panic("invalid PCI read of size %d\n", sizeof(T));
      return {0};
    }

    template <typename T>
    void write(uint32_t field, T val) {
      outl(PCI_CFG_ADDR_PORT, get_address(field));
      if constexpr (sizeof(T) == 4) return outl(PCI_CFG_DATA_PORT, val);
      if constexpr (sizeof(T) == 2) return outw(PCI_CFG_DATA_PORT + (field & 2), val);
      if constexpr (sizeof(T) == 1) return outb(PCI_CFG_DATA_PORT + (field & 3), val);
      panic("invalid PCI write of size %d\n", sizeof(T));
    }
    void enable_bus_mastering(void);


    /* change some control bits, setting some, clearing others */
    void adjust_ctrl_bits(int set, int clr);
  };

  void init();

  uint32_t read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);
  void write(uint8_t bus, uint16_t dev, uint16_t func, uint32_t reg_off, uint32_t value);

  void walk_devices(ck::func<void(device *)>);

  pci::device *find_generic_device(uint16_t class_id, uint16_t subclass_id);

};  // namespace pci

#endif
