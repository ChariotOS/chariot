#include <module.h>
#include <pci.h>



#include "ahci.h"

#include <dev/driver.h>
#include <lock.h>
#include <map.h>
#include <mem.h>
#include <module.h>
#include <pci.h>
#include <phys.h>
#include <util.h>

#define SATA_SIG_ATA 0x00000101    // SATA drive
#define SATA_SIG_ATAPI 0xEB140101  // SATAPI drive
#define SATA_SIG_SEMB 0xC33C0101   // Enclosure management bridge
#define SATA_SIG_PM 0x96690101     // Port multiplier

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define AHCI_DEV_SATAPI 4

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

static map<int, struct ahci::disk *> ahci_table;
static spinlock ahci_table_lock;

// register an ahci port to a device minor number
int ahci::register_disk(struct ahci::disk *port) {
  int min = -1;
  ahci_table_lock.lock();
  /* find the smallest index */
  for (int i = 0; true; i++) {
    if (!ahci_table.contains(i)) {
      min = i;
      break;
    }
  }
  ahci_table_lock.unlock();
  return min;
}

struct ahci::disk *ahci::get_disk(int minor) {
  struct ahci::disk *disk = NULL;
  ahci_table_lock.lock();
  /* grab the port for a given minor number */
  if (ahci_table.contains(minor)) disk = ahci_table[minor];
  ahci_table_lock.unlock();
  return disk;
}

// Check device type
static int check_type(ahci::hba_port *port) {
  uint32_t ssts = port->ssts;

  uint8_t ipm = (ssts >> 8) & 0x0F;
  uint8_t det = ssts & 0x0F;

  if (det != HBA_PORT_DET_PRESENT)  // Check drive status
    return AHCI_DEV_NULL;
  if (ipm != HBA_PORT_IPM_ACTIVE) return AHCI_DEV_NULL;

  switch (port->sig) {
    case SATA_SIG_ATAPI:
      return AHCI_DEV_SATAPI;
    case SATA_SIG_SEMB:
      return AHCI_DEV_SEMB;
    case SATA_SIG_PM:
      return AHCI_DEV_PM;
    default:
      return AHCI_DEV_SATA;
  }
}

/**
 * probe_port - find all the ports and initialize them
 */
static void probe_port(ahci::hba_mem *abar) {
  // Search disk in implemented ports
  uint32_t pi = abar->pi;
  for (int i = 0; i < 32; i++) {
    if (pi & 1) {
      auto port = &abar->ports[i];
      int dt = check_type(port);
      if (dt == AHCI_DEV_SATA) {
        AHCI_INFO("SATA drive found at port %d\n", i);
        ahci::init_sata(make_unique<ahci::disk>(abar, port));
      } else if (dt == AHCI_DEV_SATAPI) {
        AHCI_INFO("SATAPI drive found at port %d\n", i);
      } else if (dt == AHCI_DEV_SEMB) {
        AHCI_INFO("SEMB drive found at port %d\n", i);
      } else if (dt == AHCI_DEV_PM) {
        AHCI_INFO("PM drive found at port %d\n", i);
      } else {
        // AHCI_INFO("No drive found at port %d\n", i);
      }
    }

    pi >>= 1;
  }
}

/**
 * init_device - using a PCI device, initialize all the drives
 */
static void init_device(pci::device *dev) {
  AHCI_INFO("Found Host Bus Adapter [%04x:%04x]\n", dev->vendor_id,
            dev->device_id);

  /* AHCI stores it's ABAR in bar5 of the HBA's PCI registers */
  u64 bar = dev->get_bar(5).raw;
  auto *abar = (ahci::hba_mem *)p2v(bar); /* do not free() */
  /* Enable bus mastering so the controller can DMA and all that. */
  dev->enable_bus_mastering();
  /* Probe the ABAR for drives */
  probe_port(abar);
}


ahci::disk::disk(struct hba_mem *abar, struct hba_port *port)
    : abar(abar), port(port) {}

volatile struct ahci::hba_cmd_hdr *ahci::disk::get_cmd_hdr(void) {
  u64 clb = port->clb | ((u64)port->clbu << 32);
  auto cmd_hdr = (struct ahci::hba_cmd_hdr *)p2v(clb);
  return cmd_hdr;
}
volatile struct ahci::hba_cmd *ahci::disk::get_cmd_table(void) {
  auto cmd_hdr = get_cmd_hdr();
  auto cmd_table =
      (struct ahci::hba_cmd *)p2v(cmd_hdr->ctba | ((u64)cmd_hdr->ctbau << 32));
  return cmd_table;
}



void ahci::disk::rebase(void) {
  /* Stop the command engine */
  port->cmd = 0;
  stop_cmd();


  // auto clb = (u64)phys::alloc(1 /* TODO */);
  // printk("clb=%p\n", clb);

  /* Start command engine back up. */
  start_cmd();
}

bool ahci::disk::read(off_t sector, uint32_t count, void *dst) {

	return true;
}


void ahci::disk::stop_cmd(void) {
  // Clear ST (bit0)
  port->cmd &= ~HBA_PxCMD_ST;

  // Wait until FR (bit14), CR (bit15) are cleared
  while (1) {
    if (port->cmd & HBA_PxCMD_FR) continue;
    if (port->cmd & HBA_PxCMD_CR) continue;
    break;
  }

  // Clear FRE (bit4)
  port->cmd &= ~HBA_PxCMD_FRE;
}

void ahci::disk::start_cmd(void) {
  // Wait until CR (bit15) is cleared
  while (port->cmd & HBA_PxCMD_CR)
    ;

  // Set FRE (bit4) and ST (bit0)
  port->cmd |= HBA_PxCMD_FRE;
  port->cmd |= HBA_PxCMD_ST;
}



#define HANDLE_DEV(vendor, device, func) \
  if (dev->is_device(vendor, device)) {  \
    func(dev);                           \
    return;                              \
  }


void intel_ahci(pci::device *dev) {
  //
  printk(KERN_INFO "intel ahci device %p\n", dev);
	init_device(dev);
}


void intel_ahci_mobile(pci::device *dev) {
  //
  printk(KERN_INFO "intel ahci mobile device %p\n", dev);
	init_device(dev);
}

#define INTEL 0x8086

static void ahci_mod_init(void) {
  pci::walk_devices([&](pci::device *dev) {
    HANDLE_DEV(INTEL, 0x2652, intel_ahci);        /* ICH6 */
    HANDLE_DEV(INTEL, 0x2653, intel_ahci);        /* ICH6M */
    HANDLE_DEV(INTEL, 0x27c1, intel_ahci);        /* ICH7 */
    HANDLE_DEV(INTEL, 0x27c5, intel_ahci);        /* ICH7M */
    HANDLE_DEV(INTEL, 0x27c3, intel_ahci);        /* ICH7R */
    HANDLE_DEV(INTEL, 0x2681, intel_ahci);        /* ESB2 */
    HANDLE_DEV(INTEL, 0x2682, intel_ahci);        /* ESB2 */
    HANDLE_DEV(INTEL, 0x2683, intel_ahci);        /* ESB2 */
    HANDLE_DEV(INTEL, 0x27c6, intel_ahci);        /* ICH7-M DH */
    HANDLE_DEV(INTEL, 0x2821, intel_ahci);        /* ICH8 */
    HANDLE_DEV(INTEL, 0x2824, intel_ahci);        /* ICH8 */
    HANDLE_DEV(INTEL, 0x2829, intel_ahci);        /* ICH8M */
    HANDLE_DEV(INTEL, 0x282a, intel_ahci);        /* ICH8M */
    HANDLE_DEV(INTEL, 0x2922, intel_ahci);        /* ICH9 */
    HANDLE_DEV(INTEL, 0x2923, intel_ahci);        /* ICH9 */
    HANDLE_DEV(INTEL, 0x2924, intel_ahci);        /* ICH9 */
    HANDLE_DEV(INTEL, 0x2925, intel_ahci);        /* ICH9 */
    HANDLE_DEV(INTEL, 0x2927, intel_ahci);        /* ICH9 */
    HANDLE_DEV(INTEL, 0x2929, intel_ahci_mobile); /* ICH9M */
    HANDLE_DEV(INTEL, 0x292a, intel_ahci_mobile); /* ICH9M */
    HANDLE_DEV(INTEL, 0x292b, intel_ahci_mobile); /* ICH9M */
    HANDLE_DEV(INTEL, 0x292c, intel_ahci_mobile); /* ICH9M */
    HANDLE_DEV(INTEL, 0x292f, intel_ahci_mobile); /* ICH9M */
    HANDLE_DEV(INTEL, 0x294d, intel_ahci);        /* ICH9 */
    HANDLE_DEV(INTEL, 0x294e, intel_ahci_mobile); /* ICH9M */
    HANDLE_DEV(INTEL, 0x502a, intel_ahci);        /* Tolapai */
    HANDLE_DEV(INTEL, 0x502b, intel_ahci);        /* Tolapai */
    HANDLE_DEV(INTEL, 0x3a05, intel_ahci);        /* ICH10 */
    HANDLE_DEV(INTEL, 0x3a22, intel_ahci);        /* ICH10 */
    HANDLE_DEV(INTEL, 0x3a25, intel_ahci);        /* ICH10 */
    HANDLE_DEV(INTEL, 0x3b22, intel_ahci);        /* PCH AHCI */
    HANDLE_DEV(INTEL, 0x3b23, intel_ahci);        /* PCH AHCI */
    HANDLE_DEV(INTEL, 0x3b24, intel_ahci);        /* PCH RAID */
    HANDLE_DEV(INTEL, 0x3b25, intel_ahci);        /* PCH RAID */
    HANDLE_DEV(INTEL, 0x3b29, intel_ahci_mobile); /* PCH M AHCI */
    HANDLE_DEV(INTEL, 0x3b2b, intel_ahci);        /* PCH RAID */
    HANDLE_DEV(INTEL, 0x3b2c, intel_ahci_mobile); /* PCH M RAID */
    HANDLE_DEV(INTEL, 0x3b2f, intel_ahci);        /* PCH AHCI */
    HANDLE_DEV(INTEL, 0x1c02, intel_ahci);        /* CPT AHCI */
    HANDLE_DEV(INTEL, 0x1c03, intel_ahci_mobile); /* CPT M AHCI */
    HANDLE_DEV(INTEL, 0x1c04, intel_ahci);        /* CPT RAID */
    HANDLE_DEV(INTEL, 0x1c05, intel_ahci_mobile); /* CPT M RAID */
    HANDLE_DEV(INTEL, 0x1c06, intel_ahci);        /* CPT RAID */
    HANDLE_DEV(INTEL, 0x1c07, intel_ahci);        /* CPT RAID */
    HANDLE_DEV(INTEL, 0x1d02, intel_ahci);        /* PBG AHCI */
    HANDLE_DEV(INTEL, 0x1d04, intel_ahci);        /* PBG RAID */
    HANDLE_DEV(INTEL, 0x1d06, intel_ahci);        /* PBG RAID */
    HANDLE_DEV(INTEL, 0x2826, intel_ahci);        /* PBG RAID */
    HANDLE_DEV(INTEL, 0x2323, intel_ahci);        /* DH89xxCC AHCI */
    HANDLE_DEV(INTEL, 0x1e02, intel_ahci);        /* Panther Point AHCI */
    HANDLE_DEV(INTEL, 0x1e03, intel_ahci_mobile); /* Panther M AHCI */
    HANDLE_DEV(INTEL, 0x1e04, intel_ahci);        /* Panther Point RAID */
    HANDLE_DEV(INTEL, 0x1e05, intel_ahci);        /* Panther Point RAID */
    HANDLE_DEV(INTEL, 0x1e06, intel_ahci);        /* Panther Point RAID */
    HANDLE_DEV(INTEL, 0x1e07, intel_ahci_mobile); /* Panther M RAID */
    HANDLE_DEV(INTEL, 0x1e0e, intel_ahci);        /* Panther Point RAID */
    HANDLE_DEV(INTEL, 0x8c02, intel_ahci);        /* Lynx Point AHCI */
    HANDLE_DEV(INTEL, 0x8c03, intel_ahci_mobile); /* Lynx M AHCI */
    HANDLE_DEV(INTEL, 0x8c04, intel_ahci);        /* Lynx Point RAID */
    HANDLE_DEV(INTEL, 0x8c05, intel_ahci_mobile); /* Lynx M RAID */
    HANDLE_DEV(INTEL, 0x8c06, intel_ahci);        /* Lynx Point RAID */
    HANDLE_DEV(INTEL, 0x8c07, intel_ahci_mobile); /* Lynx M RAID */
    HANDLE_DEV(INTEL, 0x8c0e, intel_ahci);        /* Lynx Point RAID */
    HANDLE_DEV(INTEL, 0x8c0f, intel_ahci_mobile); /* Lynx M RAID */
    HANDLE_DEV(INTEL, 0x9c02, intel_ahci_mobile); /* Lynx LP AHCI */
    HANDLE_DEV(INTEL, 0x9c03, intel_ahci_mobile); /* Lynx LP AHCI */
    HANDLE_DEV(INTEL, 0x9c04, intel_ahci_mobile); /* Lynx LP RAID */
    HANDLE_DEV(INTEL, 0x9c05, intel_ahci_mobile); /* Lynx LP RAID */
    HANDLE_DEV(INTEL, 0x9c06, intel_ahci_mobile); /* Lynx LP RAID */
    HANDLE_DEV(INTEL, 0x9c07, intel_ahci_mobile); /* Lynx LP RAID */
    HANDLE_DEV(INTEL, 0x9c0e, intel_ahci_mobile); /* Lynx LP RAID */
    HANDLE_DEV(INTEL, 0x9c0f, intel_ahci_mobile); /* Lynx LP RAID */
    HANDLE_DEV(INTEL, 0x9dd3, intel_ahci_mobile); /* Cannon Lake PCH-LP AHCI */
    HANDLE_DEV(INTEL, 0x1f22, intel_ahci);        /* Avoton AHCI */
    HANDLE_DEV(INTEL, 0x1f23, intel_ahci);        /* Avoton AHCI */
    HANDLE_DEV(INTEL, 0x1f24, intel_ahci);        /* Avoton RAID */
    HANDLE_DEV(INTEL, 0x1f25, intel_ahci);        /* Avoton RAID */
    HANDLE_DEV(INTEL, 0x1f26, intel_ahci);        /* Avoton RAID */
    HANDLE_DEV(INTEL, 0x1f27, intel_ahci);        /* Avoton RAID */
    HANDLE_DEV(INTEL, 0x1f2e, intel_ahci);        /* Avoton RAID */
    HANDLE_DEV(INTEL, 0x1f2f, intel_ahci);        /* Avoton RAID */
    HANDLE_DEV(INTEL, 0x2823, intel_ahci);        /* Wellsburg RAID */
    HANDLE_DEV(INTEL, 0x2827, intel_ahci);        /* Wellsburg RAID */
    HANDLE_DEV(INTEL, 0x8d02, intel_ahci);        /* Wellsburg AHCI */
    HANDLE_DEV(INTEL, 0x8d04, intel_ahci);        /* Wellsburg RAID */
    HANDLE_DEV(INTEL, 0x8d06, intel_ahci);        /* Wellsburg RAID */
    HANDLE_DEV(INTEL, 0x8d0e, intel_ahci);        /* Wellsburg RAID */
    HANDLE_DEV(INTEL, 0x8d62, intel_ahci);        /* Wellsburg AHCI */
    HANDLE_DEV(INTEL, 0x8d64, intel_ahci);        /* Wellsburg RAID */
    HANDLE_DEV(INTEL, 0x8d66, intel_ahci);        /* Wellsburg RAID */
    HANDLE_DEV(INTEL, 0x8d6e, intel_ahci);        /* Wellsburg RAID */
    HANDLE_DEV(INTEL, 0x23a3, intel_ahci);        /* Coleto Creek AHCI */
    HANDLE_DEV(INTEL, 0x9c83, intel_ahci_mobile); /* Wildcat LP AHCI */
    HANDLE_DEV(INTEL, 0x9c85, intel_ahci_mobile); /* Wildcat LP RAID */
    HANDLE_DEV(INTEL, 0x9c87, intel_ahci_mobile); /* Wildcat LP RAID */
    HANDLE_DEV(INTEL, 0x9c8f, intel_ahci_mobile); /* Wildcat LP RAID */
    HANDLE_DEV(INTEL, 0x8c82, intel_ahci);        /* 9 Series AHCI */
    HANDLE_DEV(INTEL, 0x8c83, intel_ahci_mobile); /* 9 Series M AHCI */
    HANDLE_DEV(INTEL, 0x8c84, intel_ahci);        /* 9 Series RAID */
    HANDLE_DEV(INTEL, 0x8c85, intel_ahci_mobile); /* 9 Series M RAID */
    HANDLE_DEV(INTEL, 0x8c86, intel_ahci);        /* 9 Series RAID */
    HANDLE_DEV(INTEL, 0x8c87, intel_ahci_mobile); /* 9 Series M RAID */
    HANDLE_DEV(INTEL, 0x8c8e, intel_ahci);        /* 9 Series RAID */
    HANDLE_DEV(INTEL, 0x8c8f, intel_ahci_mobile); /* 9 Series M RAID */
    HANDLE_DEV(INTEL, 0x9d03, intel_ahci_mobile); /* Sunrise LP AHCI */
    HANDLE_DEV(INTEL, 0x9d05, intel_ahci_mobile); /* Sunrise LP RAID */
    HANDLE_DEV(INTEL, 0x9d07, intel_ahci_mobile); /* Sunrise LP RAID */
    HANDLE_DEV(INTEL, 0xa102, intel_ahci);        /* Sunrise Point-H AHCI */
    HANDLE_DEV(INTEL, 0xa103, intel_ahci_mobile); /* Sunrise M AHCI */
    HANDLE_DEV(INTEL, 0xa105, intel_ahci);        /* Sunrise Point-H RAID */
    HANDLE_DEV(INTEL, 0xa106, intel_ahci);        /* Sunrise Point-H RAID */
    HANDLE_DEV(INTEL, 0xa107, intel_ahci_mobile); /* Sunrise M RAID */
    HANDLE_DEV(INTEL, 0xa10f, intel_ahci);        /* Sunrise Point-H RAID */
    HANDLE_DEV(INTEL, 0x2822, intel_ahci);        /* Lewisburg RAID*/
    HANDLE_DEV(INTEL, 0x2823, intel_ahci);        /* Lewisburg AHCI*/
    HANDLE_DEV(INTEL, 0x2826, intel_ahci);        /* Lewisburg RAID*/
    HANDLE_DEV(INTEL, 0x2827, intel_ahci);        /* Lewisburg RAID*/
    HANDLE_DEV(INTEL, 0xa182, intel_ahci);        /* Lewisburg AHCI*/
    HANDLE_DEV(INTEL, 0xa186, intel_ahci);        /* Lewisburg RAID*/
    HANDLE_DEV(INTEL, 0xa1d2, intel_ahci);        /* Lewisburg RAID*/
    HANDLE_DEV(INTEL, 0xa1d6, intel_ahci);        /* Lewisburg RAID*/
    HANDLE_DEV(INTEL, 0xa202, intel_ahci);        /* Lewisburg AHCI*/
    HANDLE_DEV(INTEL, 0xa206, intel_ahci);        /* Lewisburg RAID*/
    HANDLE_DEV(INTEL, 0xa252, intel_ahci);        /* Lewisburg RAID*/
    HANDLE_DEV(INTEL, 0xa256, intel_ahci);        /* Lewisburg RAID*/
    HANDLE_DEV(INTEL, 0xa356, intel_ahci);        /* Cannon Lake PCH-H RAID */
    HANDLE_DEV(INTEL, 0x0f22, intel_ahci_mobile); /* Bay Trail AHCI */
    HANDLE_DEV(INTEL, 0x0f23, intel_ahci_mobile); /* Bay Trail AHCI */
    HANDLE_DEV(INTEL, 0x22a3, intel_ahci_mobile); /* Cherry Tr. AHCI */
    HANDLE_DEV(INTEL, 0x5ae3, intel_ahci_mobile); /* ApolloLake AHCI */
    HANDLE_DEV(INTEL, 0x34d3, intel_ahci_mobile); /* Ice Lake LP AHCI */
  });
}

module_init("ahci", ahci_mod_init);
