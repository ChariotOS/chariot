#include <dev/driver.h>
#include <util.h>
#include <phys.h>
#include "ahci.h"

static ssize_t sata_read(fs::filedesc &fd, char *buf, size_t sz) {
  //
  return -1;
}
static ssize_t sata_write(fs::filedesc &fd, const char *buf, size_t sz) {
  //
  return -1;
}

struct dev::driver_ops sata_ops = {
    .read = sata_read,
    .write = sata_write,
};


int ahci::init_sata(struct ahci::disk *disk) {
  AHCI_INFO("disk: %p\n", disk);

  u64 clb = disk->port->clb | ((u64)disk->port->clbu << 32);

  // initialize the port
  disk->rebase();

  auto cmd_hdr = disk->get_cmd_hdr();
  auto cmd_table = disk->get_cmd_table();

  for (int i = 0; i < cmd_hdr->prdtl; i++) {
    auto &ent = cmd_table->prdt_entry[i];
    u64 dba = ent.dba | ((u64)ent.dbau << 32);
    void *buf = p2v(dba);
    hexdump(buf, ent.dbc);
    printk("%d: %p\n", i, dba);
  }

  AHCI_INFO("CLB=%p\n", clb);
  return -1;
}

