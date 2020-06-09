#include "ata.h"

#include <arch.h>
#include <cpu.h>
#include <dev/driver.h>
#include <dev/mbr.h>
#include <lock.h>
#include <mem.h>
#include <module.h>
#include <pci.h>
#include <phys.h>
#include <printk.h>
#include <ptr.h>
#include <sched.h>
#include <util.h>
#include <vga.h>
#include <wait.h>

#include "../majors.h"

// #define DEBUG
// #define DO_TRACE

#ifdef DEBUG
#define INFO(fmt, args...) KINFO("ATA: " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

#ifdef DO_TRACE
#define TRACE printk("[ATA]: (%d) %s\n", __LINE__, __PRETTY_FUNCTION__)
#else
#define TRACE
#endif

#define ATA_IRQ0 (32 + 14)
#define ATA_IRQ1 (32 + 15)
#define ATA_IRQ2 (32 + 11)
#define ATA_IRQ3 (32 + 9)

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_READ_DMA 0xC8
#define ATA_CMD_WRITE_DMA 0xCA

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

static int ata_dev_init(fs::blkdev& d);
static int ata_rw_block(fs::blkdev& b, void* data, int block, bool write);
waitqueue ata_wq;

struct fs::block_operations ata_blk_ops = {
    .init = ata_dev_init,
    .rw_block = ata_rw_block,
};

static struct dev::driver_info ata_driver_info {
  .name = "ata", .type = DRIVER_BLOCK, .major = MAJOR_ATA,

  .block_ops = &ata_blk_ops,
};

/**
 * TODO: use per-channel ATA mutex locks. Right now every ata drive is locked
 * the same way
 */
static spinlock drive_lock;

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
  drive_lock.lock();
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
  drive_lock.unlock();
}

dev::ata::~ata() {
  drive_lock.lock();
  TRACE;
  kfree(id_buf);
  if (m_dma_buffer != 0) {
    phys::free(m_dma_buffer);
  }
  drive_lock.unlock();
}

void dev::ata::select_device() {
  TRACE;
  device_port.out(master ? 0xA0 : 0xB0);
}

bool dev::ata::identify() {
  scoped_lock l(drive_lock);

  // select the correct device
  select_device();
  // clear the HOB bit, idk what that is.
  control_port.out(0);

  device_port.out(0xA0);
  u8 status = command_port.in();

  // not valid, no device on that bus
  if (status == 0xFF) {
    return false;
  }

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
    printk(KERN_ERROR "error identifying ATA drive. status=%02x\n", status);
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

  if (m_pci_dev != nullptr) {
    m_pci_dev->enable_bus_mastering();
    use_dma = true;

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
  } else {
		printk("can't use ata without DMA\n");
		return false;
	}

  return true;
}

bool dev::ata::read_blocks(u32 sector, u8* data, int n) {
  TRACE;

  // TODO: also check for scheduler avail
  if (use_dma) {
    return read_blocks_dma(sector, data, n);
  }

  // take a scoped lock
  scoped_lock lck(drive_lock);

  // printk("read block %d\n", sector);

  if (sector & 0xF0000000) return false;

  // select the correct device, and put bits of the address
  device_port.out((master ? 0xE0 : 0xF0) | ((sector & 0x0F000000) >> 24));
  error_port.out(0);
  // read a single sector
  sector_count_port.out(n);

  lba_low_port.out((sector & 0x00FF));
  lba_mid_port.out((sector & 0xFF00) >> 8);
  lba_high_port.out((sector & 0xFF0000) >> 16);

  // read command
  command_port.out(0x20);

  u8 status = wait();
  if (status & 0x1) {
    printk(KERN_ERROR "error reading ATA drive\n");
    return false;
  }

  auto* buf = (char*)data;

  for (u16 i = 0; i < sector_size * n; i += 2) {
    u16 d = data_port.in();

    buf[i] = d & 0xFF;
    buf[i + 1] = (d >> 8) & 0xFF;
  }

  return true;
}

bool dev::ata::write_blocks(u32 sector, const u8* buf, int n) {
  TRACE;

  // TODO: also check for scheduler avail
  if (use_dma) {
    return write_blocks_dma(sector, buf, n);
  }

  // TODO: use DMA here :^)
  scoped_lock lck(drive_lock);


  if (sector & 0xF0000000) return false;

  // select the correct device, and put bits of the address
  device_port.out((master ? 0xE0 : 0xF0) | ((sector & 0x0F000000) >> 24));
  error_port.out(0);
  // write a single sector
  sector_count_port.out(n);

  lba_low_port.out((sector & 0x00FF));
  lba_mid_port.out((sector & 0xFF00) >> 8);
  lba_high_port.out((sector & 0xFF0000) >> 16);

  // write command
  command_port.out(0x30);

  for (u16 i = 0; i < sector_size * n; i += 2) {
    u16 d = buf[i];
    d |= ((u16)buf[i + 1]) << 8;
    data_port.out(d);
  }

  flush();

  return true;
}

bool dev::ata::flush(void) {
  TRACE;
  device_port.out(master ? 0xE0 : 0xF0);
  command_port.out(0xE7);

  // TODO: schedule out while waiting?
  u8 status = command_port.in();
  while (((status & 0x80) == 0x80) && ((status & 0x01) != 0x01)) {
    status = command_port.in();
  }
  if (status & 0x1) {
    printk(KERN_ERROR "error flushing ATA drive\n");
    return false;
  }
  return true;
}

u8 dev::ata::wait(void) {
  TRACE;

  if (cpu::in_thread() && false) {
    // wait, but don't allow rude awakening
    ata_wq.wait_noint();
    return 0;
  } else {
    // TODO: schedule out while waiting?
    u8 status = command_port.in();
    while (((status & 0x80) == 0x80) && ((status & 0x01) != 0x01)) {
      status = command_port.in();
    }

    return status;
  }
  return -1;
}

u64 dev::ata::sector_count(void) {
  TRACE;
  return n_sectors;
}

size_t dev::ata::block_size() {
  TRACE;
  return sector_size;
}

size_t dev::ata::block_count() { return n_sectors; }

bool dev::ata::read_blocks_dma(u32 sector, u8* data, int n) {
  TRACE;

  if (sector & 0xF0000000) return false;


  scoped_lock lck(drive_lock);


	int buffer_pages = NPAGES(n * block_size() + sizeof(prdt_t));
	auto buffer = phys::alloc(buffer_pages);
  // setup the prdt for DMA
  auto* prdt = static_cast<prdt_t*>(p2v(buffer));
  prdt->transfer_size = sector_size;
  prdt->buffer_phys = (u64)buffer + sizeof(prdt_t);
  prdt->mark_end = 0x8000;

  u8* dma_dst = (u8*)p2v(prdt->buffer_phys);

  // stop bus master
  outb(bmr_command, 0);
  // Set prdt
  outl(bmr_prdt, (u64)buffer);

  // Turn on "Interrupt" and "Error" flag. The error flag should be cleared by
  // hardware.
  outb(bmr_status, inb(bmr_status) | 0x6);

  // set transfer direction
  outb(bmr_command, 0x8);

  // select the correct device, and put bits of the address
  device_port.out((master ? 0xE0 : 0xF0) | ((sector & 0x0F000000) >> 24));
  // clear the error port
  error_port.out(0);
  // read a single sector
  sector_count_port.out(n);



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

  memcpy(data, dma_dst, sector_size * n);

	phys::free(buffer, buffer_pages);

  return true;
}
bool dev::ata::write_blocks_dma(u32 sector, const u8* data, int n) {

  if (sector & 0xF0000000) return false;
  drive_lock.lock();

	int buffer_pages = NPAGES(n * block_size() + sizeof(prdt_t));
	auto buffer = phys::alloc(buffer_pages);
  // setup the prdt for DMA
  auto* prdt = static_cast<prdt_t*>(p2v(buffer));
  prdt->transfer_size = sector_size;
  prdt->buffer_phys = (u64)buffer + sizeof(prdt_t);
  prdt->mark_end = 0x8000;

  u8* dma_dst = (u8*)p2v(prdt->buffer_phys);

  // stop bus master
  outb(bmr_command, 0);
  // Set prdt
  outl(bmr_prdt, (u64)m_dma_buffer);

  // Turn on "Interrupt" and "Error" flag. The error flag should be cleared by
  // hardware.
  outb(bmr_status, inb(bmr_status) | 0x6);

  // set transfer direction
  outb(bmr_command, 0x8);

  // select the correct device, and put bits of the address
  device_port.out((master ? 0xE0 : 0xF0) | ((sector & 0x0F000000) >> 24));
  // clear the error port
  error_port.out(0);
  // read a single sector
  sector_count_port.out(n);

  lba_low_port.out((sector & 0x00FF));
  lba_mid_port.out((sector & 0xFF00) >> 8);
  lba_high_port.out((sector & 0xFF0000) >> 16);

  // copy our data
  memcpy(dma_dst, data, sector_size * n);

  // read DMA command
  command_port.out(ATA_CMD_WRITE_DMA);

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
	phys::free(buffer, buffer_pages);

  drive_lock.unlock();


	return true;
}


static void ata_interrupt(int intr, reg_t* fr) {
  inb(primary_master_status);
  inb(primary_master_bmr_status);

  outb(primary_master_bmr_status, BMR_COMMAND_DMA_STOP);

  if (sched::enabled()) {
    ata_wq.notify();
  }

  irq::eoi(intr);
  // INFO("interrupt: err=%d\n", fr->err);
}

static vec<ref<dev::disk>> m_disks;

static void add_drive(const string& name, ref<dev::disk> drive) {
  int minor = m_disks.size();
  // KINFO("Detected ATA drive '%s'\n", name.get(), MAJOR_ATA);
  m_disks.push(drive);


  dev::register_name(ata_driver_info, name, minor);
}

static void query_and_add_drive(u16 addr, int id, bool master) {
  auto drive = make_ref<dev::ata>(addr, master);

  if (drive->identify()) {
    // auto name = dev::next_disk_name();
    string name = string::format("ata%d", id);

    // add the main drive
    add_drive(name, drive);

    // detect mbr partitions
    void* first_sector = kmalloc(drive->block_size());

    drive->read_blocks(0, (u8*)first_sector, 1);

    if (dev::mbr mbr; mbr.parse(first_sector)) {
      for (int i = 0; i < mbr.part_count(); i++) {
        auto part = mbr.partition(i);
        auto pname = string::format("%sp%d", name.get(), i + 1);

        auto part_disk = make_ref<dev::ata_part>(drive, part.off, part.len);

        add_drive(pname, part_disk);
      }
    }

    kfree(first_sector);
  }
}
static void ata_initialize(void) {
  // TODO: make a new IRQ dispatch system to make this more general
  irq::install(ATA_IRQ0, ata_interrupt, "ATA Drive");
  // smp::ioapicenable(ATA_IRQ0, 0);

  irq::install(ATA_IRQ1, ata_interrupt, "ATA Drive");
  // smp::ioapicenable(ATA_IRQ1, 0);

  // register all the ata drives on the system
  query_and_add_drive(0x1F0, 0, true);
  query_and_add_drive(0x1F0, 1, false);
}

static dev::disk* get_disk(int minor) {
  if (minor >= 0 && minor < m_disks.size()) {
    return m_disks[minor].get();
  }
  printk("inval\n");
  return nullptr;
}

dev::ata_part::~ata_part() {}

bool dev::ata_part::read_blocks(u32 sector, u8* data, int n) {
  if (sector > len) return false;
  return parent->read_blocks(sector + start, data, n);
}

bool dev::ata_part::write_blocks(u32 sector, const u8* data, int n) {
  if (sector > len) return false;
  return parent->write_blocks(sector + start, data, n);
}

size_t dev::ata_part::block_size(void) { return parent->block_size(); }

size_t dev::ata_part::block_count(void) { return parent->block_count(); }

static int ata_dev_init(fs::blkdev& d) {
  auto disk = get_disk(d.dev.minor());

  d.block_count = disk->block_count();
  d.block_size = disk->block_size();

  return 0;
}

static int ata_rw_block(fs::blkdev& b, void* data, int block, bool write) {
  // printk("ata %c %d %p\n", write ? 'w' : 'r', block, data);
  auto d = get_disk(b.dev.minor());
  if (d == NULL) return -1;

  bool success = false;
  if (write) {
    success = d->write_blocks(block, (const u8*)data);
  } else {
    success = d->read_blocks(block, (u8*)data);
  }
  return success ? 0 : -1;
}

static void ata_init(void) {
  ata_initialize();

  if (dev::register_driver(ata_driver_info) != 0) {
    panic("failed to register ATA driver\n");
  }
}

module_init("ata", ata_init);
