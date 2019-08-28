#include <dev/ata.h>
#include <fs/devfs.h>
#include <idt.h>
#include <mem.h>
#include <module.h>
#include <pci.h>
#include <phys.h>
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

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_READ_DMA 0xC8

#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCOUNT0 0x02
#define ATA_REG_LBA0 0x03
#define ATA_REG_LBA1 0x04
#define ATA_REG_LBA2 0x05
#define ATA_REG_HDDEVSEL 0x06
#define ATA_REG_COMMAND 0x07
#define ATA_REG_STATUS 0x07
#define ATA_REG_SECCOUNT1 0x08
#define ATA_REG_LBA3 0x09
#define ATA_REG_LBA4 0x0A
#define ATA_REG_LBA5 0x0B
#define ATA_REG_CONTROL 0x0C
#define ATA_REG_ALTSTATUS 0x0C
#define ATA_REG_DEVADDRESS 0x0D

// Bus Master Reg Command
#define BMR_COMMAND_DMA_START 0x1
#define BMR_COMMAND_DMA_STOP 0x0
#define BMR_COMMAND_READ 0x8
#define BMR_STATUS_INT 0x4
#define BMR_STATUS_ERR 0x2

/*
 * TODO: determine if we need this function
static void wait_400ns(u16 io_base) {
  for (int i = 0; i < 4; ++i) inb(io_base + ATA_REG_ALTSTATUS);
}
*/

// for the interrupts...
u16 primary_master_status = 0;
u16 primary_master_bmr_status = 0;
u16 primary_master_bmr_command = 0;

dev::ata::ata(u16 portbase, bool master) {

  m_io_base = portbase;
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

  m_dma_buffer = nullptr;
}

dev::ata::~ata() {
  TRACE;
  kfree(id_buf);
  if (m_dma_buffer != 0) {
    phys::free(m_dma_buffer);
  }
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

  m_pci_dev = nullptr;

  // find the ATA controller on the PCI system
  pci::walk_devices([&](pci::device* dev) -> void {
    if (dev->is_device(0x8086, 0x7010) || dev->is_device(0x8086, 0x7111)) {
      m_pci_dev = dev;
    }
  });

  if (m_pci_dev == nullptr) {
    panic("no ATA PCI controller found!\n");
  }

  m_pci_dev->enable_bus_mastering();
  use_dma = true;

  // allocate the physical page for the dma buffer
  m_dma_buffer = phys::alloc();

  // bar4 contains information for DMA
  bar4 = m_pci_dev->get_bar(4).raw;
  if (bar4 & 0x1) bar4 = bar4 & 0xfffffffc;

  bmr_command = bar4;
  bmr_status = bar4 + 2;
  bmr_prdt = bar4 + 4;

  if (this->master && data_port.m_port == 0x1F0) {
    // printk("PRIMARY MASTER\n");
    primary_master_status = m_io_base;
    primary_master_bmr_status = bmr_status;
    primary_master_bmr_command = bmr_command;
  }

  return true;
}

bool dev::ata::read_block(u32 sector, u8* data) {
  TRACE;


  // TODO: also check for scheduler avail
  if (use_dma) {
    return read_block_dma(sector, data);
  }

  // TODO: take a lock

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

  // TODO: take a lock.

  if (sector & 0xF0000000) return false;


  // setup the prdt for DMA
  auto* prdt = static_cast<prdt_t*>(p2v(m_dma_buffer));
  prdt->transfer_size = sector_size;
  prdt->buffer_phys = (u64)m_dma_buffer + sizeof(prdt_t);
  prdt->mark_end = 0x8000;

  u8* dma_dst = (u8*)p2v(prdt->buffer_phys);

  // stop bus master
  outb(bmr_command, 0);
  // Set prdt
  outl(bmr_prdt, (u64)m_dma_buffer);

  // Turn on "Interrupt" and "Error" flag. The error flag should be cleared by hardware.
  outb(bmr_status, inb(bmr_status) | 0x6);

  // set transfer direction
  outb(bmr_command, 0x8);


  // select the correct device, and put bits of the address
  device_port.out((master ? 0xE0 : 0xF0) | ((sector & 0x0F000000) >> 24));
  // clear the error port
  error_port.out(0);
  // read a single sector
  sector_count_port.out(1);

  lba_low_port.out((sector & 0x00FF));
  lba_mid_port.out((sector & 0xFF00) >> 8);
  lba_high_port.out((sector & 0xFF0000) >> 16);

  // read DMA command
  command_port.out(0xC8);

  // start bus master
  outb(bmr_command, 0x9);

  int i = 0;

  while (1) {
    i++;
    auto status = inb(bmr_status);
    auto dstatus = command_port.in();
    if (!(status & 0x04)) {
      continue;
    }
    if (!(dstatus & 0x80)) {
      break;
    }
  }

  // wait_400ns(m_io_base);
  // printk("loops: %d\n", i);

  memcpy(data, dma_dst, sector_size);

  return true;
}
bool dev::ata::write_block_dma(u32 sector, const u8* data) { return false; }

static void ata_interrupt(int intr, trapframe* fr) {
  inb(primary_master_status);
  inb(primary_master_bmr_status);
  outb(primary_master_bmr_status, BMR_COMMAND_DMA_STOP);
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
