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
        ahci::init_sata(new ahci::disk(abar, port));
      } else if (dt == AHCI_DEV_SATAPI) {
        AHCI_INFO("SATAPI drive found at port %d\n", i);
      } else if (dt == AHCI_DEV_SEMB) {
        AHCI_INFO("SEMB drive found at port %d\n", i);
      } else if (dt == AHCI_DEV_PM) {
        AHCI_INFO("PM drive found at port %d\n", i);
      } else {
        AHCI_INFO("No drive found at port %d\n", i);
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
    : abar(abar), port(port) {
}

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


  auto clb = (u64)phys::alloc(1 /* TODO */);
  printk("clb=%p\n", clb);

  /* Start command engine back up. */
  start_cmd();
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
/**
 * ahci_init - walk PCI devices for HBAs and initialize them
 */
void ahci_init(void) {
  /* Find a PCI device with class_id=0x01 (Mass Storage), subclass=0x06
   * (Serial ATA) */
  pci::walk_devices([](pci::device *dev) {
    if (dev->class_id == 0x01 && dev->subclass_id == 0x06) {
      init_device(dev);
    }
  });
}

module_init("AHCI", ahci_init);
