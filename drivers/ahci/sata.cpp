#include <phys.h>
#include <util.h>
#include "ahci.h"

int ahci::init_sata(unique_ptr<struct ahci::disk> disk) {
  return -1;
  AHCI_INFO("disk: %p\n", disk.get());

  u64 clb = disk->port->clb | ((u64)disk->port->clbu << 32);

  // initialize the port
  disk->rebase();

  auto cmd_hdr = disk->get_cmd_hdr();
  auto cmd_table = disk->get_cmd_table();

  for (int i = 0; i < cmd_hdr->prdtl; i++) {
    auto &ent = cmd_table->prdt_entry[i];
    u64 dba = ent.dba | ((u64)ent.dbau << 32);
    void *buf = p2v(dba);
    printk("%d: %p\n", i, dba);
    hexdump(buf, ent.dbc, true);
    printk("\n");
  }

  // char buf[512];
  // memset(buf, 0, 512);
  // disk->read(0, 1, buf);
  // hexdump(buf, 512, true);

  AHCI_INFO("CLB=%p\n", clb);
  return -1;
}
