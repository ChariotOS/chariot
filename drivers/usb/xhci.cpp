#include <dev/usb/host_xhci.h>
#include <mem.h>
#include <module.h>
#include <pci.h>
#include <sleep.h>
#include <util.h>
#include <vec.h>

#define REG_CAPLENGTH 0x00
#define REG_HCIVERSION 0x02
#define REG_HCSPARAMS1 0x04
#define REG_HCSPARAMS2 0x08
#define REG_HCSPARAMS3 0x0c
#define REG_HCCPARAMS1 0x10
#define REG_DOORBELLOFF 0x14
#define REG_RUNTIMEOFF 0x18
#define REG_HCCPARAMS2 0x1c

#define XHCI_DEBUG KERN_DEBUG "[XHCI] "
#define XHCI_WARN KERN_WARN "[XHCI] "



#define NUM_PORT_REGS 4

struct xhci_op_regs {
  uint32_t command;
  uint32_t status;
  uint32_t page_size;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t dev_notification;
  uint64_t cmd_ring;
  /* rsvd: offset 0x20-2F */
  uint32_t reserved3[4];
  uint64_t dcbaa_ptr;
  uint32_t config_reg;
  /* rsvd: offset 0x3C-3FF */
  uint32_t reserved4[241];
  /* port 1 registers, which serve as a base address for other ports */
  uint32_t port_status_base;
  uint32_t port_power_base;
  uint32_t port_link_base;
  uint32_t reserved5;
  /* registers for ports 2-255 */
  uint32_t reserved6[NUM_PORT_REGS * 254];
} __packed;


static ck::vec<ck::unique_ptr<xhci>> xhci_devices;

void xhci_init(void) {
  pci::walk_devices([&](pci::device *dev) {
    // check if the device is an e1000 device

    // All xHCI controllers will have a Class ID of 0x0C and a Sublcass ID of 0x03
    if (dev->class_id == 0x0C && dev->subclass_id == 0x03) {
      xhci_devices.push(ck::make_unique<xhci>(dev));
      printk(KERN_DEBUG "\n");
      printk(KERN_DEBUG "\n");
    }
  });


  for (int i = 0; i < xhci_devices.size(); i++) {
    auto &xhci = xhci_devices[i];
    if (xhci->initialized) {
      printk(XHCI_DEBUG "Device %d initialized.\n", i);
    } else {
      printk(XHCI_WARN "Device %d not initialized.\n", i);
    }
  }

#if 0
  auto *ops = (volatile struct xhci_op_regs *)p2v(addr + regs->caplength);

  uint32_t cparams = regs->hcc_params1;
  if (cparams == 0xffffffff) return;
  printk(XHCI_DEBUG "capability parameters: 0x%08x\n", cparams);

	int cap_register_offset = 0;
	auto *caps = (volatile uint32_t*)((uint8_t*)regs + cap_register_offset);

	// if 64 bytes context structures, then 1
	int context_size_shift = HCC_CSZ(cparams);

	/* Assume ownership of the controller from the BIOS */
	uint32_t eec = 0xFFFF'FFFF;
	uint32_t eecp = HCS0_XECP(cparams) << 2;

	for (; eecp != 0 && XECP_NEXT(eec); eecp += XECP_NEXT(eec) << 2) {
		printk(XHCI_DEBUG "eecp register 0x%08x\n", eecp);

	}
  // hexdump((void*)ops, sizeof(*ops), true);
#endif
}

module_init("eXtensible Host Controller Interface", xhci_init);




xhci::xhci(pci::device *dev) : dev(dev), cmd_complete(0), finish_transfers(0), event(0) {
  scoped_lock l(lock);

  dev->enable_bus_mastering();

  uint64_t addr = ((uint64_t)dev->get_bar(0).addr) | ((uint64_t)dev->get_bar(1).addr << 32);
  printk(XHCI_DEBUG "registers at %p\n", addr);

  regs = (volatile struct xhci_regs *)p2v(addr);
  /*
printk(XHCI_DEBUG "caplength: %x\n", regs->caplength);
printk(XHCI_DEBUG "hci_version: %x\n", regs->hci_version);
printk(XHCI_DEBUG "hcs_params1: %x\n", regs->hcs_params1);
printk(XHCI_DEBUG "hcs_params2: %x\n", regs->hcs_params2);
printk(XHCI_DEBUG "hcs_params3: %x\n", regs->hcs_params3);
printk(XHCI_DEBUG "hcc_params1: %x\n", regs->hcc_params1);
printk(XHCI_DEBUG "db_off:      %x\n", regs->db_off);
printk(XHCI_DEBUG "rts_off:     %x\n", regs->rts_off);
  */

  cap_register_offset = 0;
  op_register_offset = regs->caplength;

  uint32_t cparams = read_cap_reg32(XHCI_HCCPARAMS);
  if (cparams == 0xffffffff) return;

  printk(XHCI_DEBUG "Capability parameters: %08x\n", cparams);

  context_size_shift = HCC_CSZ(cparams);

  /* Take ownership from the BIOS */
  uint32_t eec = 0xFFFF'FFFF;
  uint32_t eecp = HCS0_XECP(cparams) << 2;
  for (; eecp != 0 && XECP_NEXT(eec); eecp += XECP_NEXT(eec) << 2) {
    printk(XHCI_DEBUG "eecp register: 0x%08x\n", eecp);
    eec = read_cap_reg32(eecp);
    if (XECP_ID(eec) != XHCI_LEGSUP_CAPID) continue;

    if (eec & XHCI_LEGSUP_BIOSOWNED) {
      printk(XHCI_DEBUG "the host controller is bios owned, claiming ownership\n");
      write_cap_reg32(eecp, eec | XHCI_LEGSUP_OSOWNED);

      for (int32_t i = 0; i < 20; i++) {
        eec = read_cap_reg32(eecp);

        if ((eec & XHCI_LEGSUP_BIOSOWNED) == 0) break;

        printk(XHCI_DEBUG "controller is still bios owned, waiting\n");
        do_usleep(50000);
      }

      if (eec & XHCI_LEGSUP_BIOSOWNED) {
        printk(XHCI_DEBUG
            "bios won't give up control over the host "
            "controller (ignoring)\n");
      } else if (eec & XHCI_LEGSUP_OSOWNED) {
        printk(XHCI_DEBUG
            "successfully took ownership of the host "
            "controller\n");
      }

      // Force off the BIOS owned flag, and clear all SMIs. Some BIOSes
      // do indicate a successful handover but do not remove their SMIs
      // and then freeze the system when interrupts are generated.
      write_cap_reg32(eecp, eec & ~XHCI_LEGSUP_BIOSOWNED);
    }
    break;
  }

  uint32_t legctlsts = read_cap_reg32(eecp + XHCI_LEGCTLSTS);
  legctlsts &= XHCI_LEGCTLSTS_DISABLE_SMI;
  legctlsts |= XHCI_LEGCTLSTS_EVENTS_SMI;
  write_cap_reg32(eecp + XHCI_LEGCTLSTS, legctlsts);

  // We need to explicitly take ownership of EHCI ports on earlier Intel chipsets.
  if (dev->vendor_id == PCI_VENDOR_INTEL) {
    switch (dev->device_id) {
      case PCI_DEVICE_INTEL_PANTHER_POINT_XHCI:
      case PCI_DEVICE_INTEL_LYNX_POINT_XHCI:
      case PCI_DEVICE_INTEL_LYNX_POINT_LP_XHCI:
      case PCI_DEVICE_INTEL_BAYTRAIL_XHCI:
      case PCI_DEVICE_INTEL_WILDCAT_POINT_XHCI:
      case PCI_DEVICE_INTEL_WILDCAT_POINT_LP_XHCI:
        printk(XHCI_DEBUG "NEED TO IMPLEMENT SwitchIntelPorts()!\n");
        // _SwitchIntelPorts();
        break;
    }
  }


  if (!controller_halt()) {
    return;
  }

  if (!controller_reset()) {
    printk(XHCI_WARN "host controller failed to reset\n");
    return;
  }


  /* TODO: make event thread and finisher thread */


  /* Find the right interrupt vector, using MSIs if avail */
  this->irq = dev->interrupt;
  printk(XHCI_DEBUG "irq: %d\n", irq);

  initialized = true;
}

xhci::~xhci(void) { printk(XHCI_DEBUG "~xhci\n"); }

uint32_t xhci::read_cap_reg32(uint32_t reg) {
  /* Read the register */
  return *(volatile uint32_t *)((off_t)regs + cap_register_offset + reg);
}
void xhci::write_cap_reg32(uint32_t reg, uint32_t val) {
  /* Write the register */
  *(volatile uint32_t *)((off_t)regs + cap_register_offset + reg) = val;
}

uint32_t xhci::read_op_reg32(uint32_t reg) {
  /* Read the register */
  return *(volatile uint32_t *)((off_t)regs + op_register_offset + reg);
}
void xhci::write_op_reg32(uint32_t reg, uint32_t val) {
  /* Write the register */
  *(volatile uint32_t *)((off_t)regs + op_register_offset + reg) = val;
}

bool xhci::controller_halt(void) {
  // Mask off run state
  write_op_reg32(XHCI_CMD, read_op_reg32(XHCI_CMD) & ~CMD_RUN);

  // wait for shutdown state
  if (!wait_op_bits(XHCI_STS, STS_HCH, STS_HCH)) {
    printk(XHCI_WARN "HCH shutdown timeout\n");
    return false;
  }
  return true;
}

bool xhci::controller_reset(void) {
  printk(XHCI_DEBUG "ControllerReset() cmd: 0x%08x sts: 0x%08x\n", read_op_reg32(XHCI_CMD),
      read_op_reg32(XHCI_STS));
  write_op_reg32(XHCI_CMD, read_op_reg32(XHCI_CMD) | CMD_HCRST);

  if (!wait_op_bits(XHCI_CMD, CMD_HCRST, 0)) {
    printk(XHCI_WARN "ControllerReset() failed CMD_HCRST\n");
    return false;
  }

  if (!wait_op_bits(XHCI_STS, STS_CNR, 0)) {
    printk(XHCI_WARN "ControllerReset() failed STS_CNR\n");
    return false;
  }

  return true;
}

bool xhci::wait_op_bits(uint32_t reg, uint32_t mask, uint32_t expected) {
  int loops = 0;
  uint32_t value = read_op_reg32(reg);
  while ((value & mask) != expected) {
    do_usleep(1000);
    value = read_op_reg32(reg);
    if (loops == 100) {
      printk(XHCI_DEBUG "delay waiting on reg 0x%08x match 0x%08x (0x%08x)\n", reg, expected, mask);
    } else if (loops > 250) {
      printk(
          XHCI_DEBUG "timeout waiting on reg 0x%08x match 0x%08x (0x%08x)\n", reg, expected, mask);
      return false;
    }
    loops++;
  }
  return true;
}
