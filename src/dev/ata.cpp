#include <dev/ata.h>
#include <fs/devfs.h>
#include <idt.h>
#include <mem.h>
#include <module.h>
#include <printk.h>
#include <ptr.h>

// #define DEBUG
// #define DO_TRACE

#ifdef DEBUG
#define INFO(fmt, args...) printk("[ATA] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

#ifdef DO_TRACE
#define TRACE INFO("TRACE: (%d) %s\n", __LINE__, __PRETTY_FUNCTION__)
#else
#define TRACE
#endif

#define ATA_IRQ0 (32 + 14)
#define ATA_IRQ1 (32 + 15)
#define ATA_IRQ2 (32 + 11)
#define ATA_IRQ3 (32 + 9)

dev::ata::ata(u16 portbase, bool master) {
  TRACE;
  sector_size = 512;
  this->master = master;

  data_port = portbase;
  error_port = portbase + 1;
  sector_count_port = portbase + 2;
  lba_low_port = portbase + 3;
  lba_mid_port = portbase + 4;
  lba_high_port = portbase + 5;
  device_port = portbase + 6;
  command_port = portbase + 7;
  control_port = portbase + 0x206;
}

dev::ata::~ata() {
  TRACE;
  kfree(id_buf);
}

void dev::ata::select_device() {
  TRACE;
  device_port.out(master ? 0xA0 : 0xB0);
}

bool dev::ata::identify() {
  TRACE;
  // select the correct device
  select_device();
  // clear the HOB bit, idk what that is.
  control_port.out(0);

  device_port.out(0xA0);
  u8 status = command_port.in();

  // not valid, no device on that bus
  if (status == 0xFF) return false;

  select_device();
  sector_count_port.out(0);
  lba_low_port.out(0);
  lba_mid_port.out(0);
  lba_high_port.out(0);

  // command for identify
  command_port.out(0xEC);

  status = command_port.in();
  if (status == 0x00) {
    return false;
  }

  // read until the device is ready
  while (((status & 0x80) == 0x80) && ((status & 0x01) != 0x01)) {
    status = command_port.in();
  }

  if (status & 0x01) {
    printk("error identifying ATA drive. status=%02x\n", status);
    return false;
  }

  id_buf = (u16*)kmalloc(sizeof(u16) * 256);
  for (u16 i = 0; i < 256; i++) id_buf[i] = data_port.in();

  u8 C = id_buf[1];
  u8 H = id_buf[3];
  u8 S = id_buf[6];

  n_sectors = (C * H) * S;

  return true;
}

bool dev::ata::read_block(u32 sector, u8* data) {
  TRACE;

  // printk("read block %d\n", sector);

  if (sector & 0xF0000000) return false;

  // select the correct device, and put bits of the address
  device_port.out((master ? 0xE0 : 0xF0) | ((sector & 0x0F000000) >> 24));
  error_port.out(0);
  // read a single sector
  sector_count_port.out(1);

  lba_low_port.out((sector & 0x00FF));
  lba_mid_port.out((sector & 0xFF00) >> 8);
  lba_high_port.out((sector & 0xFF0000) >> 16);

  // read command
  command_port.out(0x20);

  u8 status = wait();
  if (status & 0x1) {
    printk("error reading ATA drive\n");
    return false;
  }

  auto* buf = (char*)data;

  for (u16 i = 0; i < sector_size; i += 2) {
    u16 d = data_port.in();

    buf[i] = d & 0xFF;
    buf[i + 1] = (d >> 8) & 0xFF;
  }

  return true;
}

bool dev::ata::write_block(u32 sector, const u8* buf) {
  TRACE;

  printk("write block %d\n", sector);

  if (sector & 0xF0000000) return false;

  // select the correct device, and put bits of the address
  device_port.out((master ? 0xE0 : 0xF0) | ((sector & 0x0F000000) >> 24));
  error_port.out(0);
  // write a single sector
  sector_count_port.out(1);

  lba_low_port.out((sector & 0x00FF));
  lba_mid_port.out((sector & 0xFF00) >> 8);
  lba_high_port.out((sector & 0xFF0000) >> 16);

  // write command
  command_port.out(0x30);

  for (u16 i = 0; i < sector_size; i += 2) {
    u16 d = buf[i];
    d |= ((u16)buf[i + 1]) << 8;
    data_port.out(d);
  }

  return true;
}

bool dev::ata::flush(void) {
  TRACE;
  device_port.out(master ? 0xE0 : 0xF0);
  command_port.out(0xE7);

  u8 status = wait();
  if (status & 0x1) {
    printk("error flushing ATA drive\n");
    return false;
  }
  return true;
}

u8 dev::ata::wait(void) {
  TRACE;
  u8 status = command_port.in();
  while (((status & 0x80) == 0x80) && ((status & 0x01) != 0x01)) {
    status = command_port.in();
  }
  return status;
}

u64 dev::ata::sector_count(void) {
  TRACE;
  return n_sectors;
}

u64 dev::ata::block_size() {
  TRACE;
  return sector_size;
}



bool dev::ata::read_block_dma(u32 sector, u8* data) {
  TRACE;

  if (sector & 0xF0000000) return false;

  // select the correct device, and put bits of the address
  device_port.out((master ? 0xE0 : 0xF0) | ((sector & 0x0F000000) >> 24));
  error_port.out(0);
  // read a single sector
  sector_count_port.out(1);

  lba_low_port.out((sector & 0x00FF));
  lba_mid_port.out((sector & 0xFF00) >> 8);
  lba_high_port.out((sector & 0xFF0000) >> 16);




  // read command
  command_port.out(0x20);

  u8 status = wait();
  if (status & 0x1) {
    printk("error reading ATA drive\n");
    return false;
  }

  auto* buf = (char*)data;

  for (u16 i = 0; i < sector_size; i += 2) {
    u16 d = data_port.in();

    buf[i] = d & 0xFF;
    buf[i + 1] = (d >> 8) & 0xFF;
  }

  return true;

}
bool dev::ata::write_block_dma(u32 sector, const u8* data) { return false; }



static void ata_interrupt(int intr, trapframe* fr) {
  // INFO("interrupt: err=%d\n", fr->err);
}

static bool find_disk(u16 addr, int id, bool master) {
  auto drive = make_unique<dev::ata>(addr, master);

  if (drive->identify()) {
    string name = string::format("disk%d", id);
    INFO("found disk %s\n", name.get());

    fs::devfs::register_device(name, move(drive), DEVFS_REG_WALK_PARTS);
    return true;
  }
  return false;
}

void ata_init(void) {
  interrupt_register(ATA_IRQ0, ata_interrupt);
  interrupt_enable(ATA_IRQ0);

  interrupt_register(ATA_IRQ1, ata_interrupt);
  interrupt_enable(ATA_IRQ1);

  // try to load hard disks at their expected locations
  find_disk(0x1F0, 0, true);
  find_disk(0x1F0, 1, false);
}

module_init("ata", ata_init);
