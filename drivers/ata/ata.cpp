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
#include <smp.h>
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

waitqueue ata_wq("ata");

/**
 * TODO: use per-channel ATA mutex locks. Right now every ata drive is locked
 * the same way
 */
static spinlock drive_lock = spinlock("ata::drive_lock");

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

dev::ata::ata(u16 portbase, bool master) : dev::blk_dev(nullptr) {
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
  drive_lock.lock();

  // select the correct device
  select_device();
  // clear the HOB bit, idk what that is.
  control_port.out(0);

  device_port.out(0xA0);
  u8 status = command_port.in();

  // not valid, no device on that bus
  if (status == 0xFF) {
    drive_lock.unlock();
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
    drive_lock.unlock();
    return false;
  }

  // read until the device is ready
  while (((status & 0x80) == 0x80) && ((status & 0x01) != 0x01)) {
    status = command_port.in();
  }

  if (status & 0x01) {
    printk("error identifying ATA drive. status=%02x\n", status);
    drive_lock.unlock();
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
    use_dma = false;

    // allocate the physical page for the dma buffer
    m_dma_buffer = phys::alloc();

    // bar4 contains information for DMA
    bar4 = m_pci_dev->get_bar(4).raw;
    printk("bar4: %08x\n", bar4);
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
  }

  drive_lock.unlock();
  return true;
}

bool dev::ata::read_block(u32 sector, u8* data) {
  TRACE;

  // TODO: also check for scheduler avail
  if (use_dma) {
    return read_block_dma(sector, data);
  }

  // take a scoped lock
  scoped_lock lck(drive_lock);

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
  scoped_lock lck(drive_lock);

  // hexdump((void*)buf, 512);

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
    printk("error flushing ATA drive\n");
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

ssize_t dev::ata::size() { return sector_size * n_sectors; }

bool dev::ata::read_block_dma(u32 sector, u8* data) {
  TRACE;

  printk("ata read: %d\n", sector);
  if (sector & 0xF0000000) return false;

  drive_lock.lock();

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
  sector_count_port.out(1);

  lba_low_port.out((sector & 0x00FF));
  lba_mid_port.out((sector & 0xFF00) >> 8);
  lba_high_port.out((sector & 0xFF0000) >> 16);

  // read DMA command
  command_port.out(0xC8);

  // start bus master
  outb(bmr_command, 0x9);

  this->wait();

  memcpy(data, dma_dst, sector_size);

  drive_lock.unlock();

  hexdump(data, 512);

  return true;
}
bool dev::ata::write_block_dma(u32 sector, const u8* data) { return false; }

static void ata_interrupt(int intr, struct task_regs* fr) {
  // irq::eoi(intr);
  inb(primary_master_status);
  inb(primary_master_bmr_status);
  outb(primary_master_bmr_status, BMR_COMMAND_DMA_STOP);

  if (sched::enabled()) {
    ata_wq.notify();
  }
  // INFO("interrupt: err=%d\n", fr->err);
}

static vec<ref<dev::blk_dev>> m_disks;

static void add_drive(const string& name, ref<dev::blk_dev> drive) {
  KINFO("Detected ATA drive '%s' (%d,%d) %zu bytes\n", name.get(), MAJOR_ATA,
        m_disks.size(), drive->size());
  dev::register_name(name, MAJOR_ATA, m_disks.size());
  m_disks.push(drive);
}

static void query_and_add_drive(u16 addr, int id, bool master) {
  auto drive = make_ref<dev::ata>(addr, master);

  if (drive->identify()) {
    // auto name = dev::next_disk_name();
    string name = string::format("ata%d", id);

    // add the main drive
    add_drive(name, drive);

    // now detect all the mbr partitions
    if (dev::mbr mbr(*drive); mbr.parse()) {
      for (int i = 0; i < mbr.part_count(); i++) {
        auto pname = string::format("%sp%d", name.get(), i + 1);
        add_drive(pname, mbr.partition(i));
      }
    }
  }
}
static void ata_initialize(void) {
  // TODO: make a new IRQ dispatch system to make this more general
  irq::install(32 + ATA_IRQ0, ata_interrupt, "ATA Drive");
  smp::ioapicenable(ATA_IRQ0, 0);

  irq::install(32 + ATA_IRQ1, ata_interrupt, "ATA Drive");
  smp::ioapicenable(ATA_IRQ1, 0);

  // register all the ata drives on the system
  query_and_add_drive(0x1F0, 0, true);
  query_and_add_drive(0x1F0, 1, false);
}

static dev::blk_dev* get_disk(int minor) {
  if (minor >= 0 && minor < m_disks.size()) {
    return m_disks[minor].get();
  }
  return nullptr;
}

static ssize_t ata_read(fs::filedesc& fd, char* buf, size_t sz) {
  if (fd) {
    auto d = get_disk(fd.ino->minor);
    if (d == NULL) return -1;
    auto k = d->read(fd.offset(), sz, buf);
    if (k) fd.seek(k);
    return sz;
  }
  return -1;
}

static ssize_t ata_write(fs::filedesc& fd, const char* buf, size_t sz) {
  if (fd) {
    auto d = get_disk(fd.ino->minor);
    if (d == NULL) return -1;
    auto k = d->write(fd.offset(), sz, buf);
    if (k) fd.seek(k);
    return sz;
  }
  return -1;
}

struct dev::driver_ops ata_ops = {
    .llseek = NULL,
    .read = ata_read,
    .write = ata_write,
    .ioctl = NULL,
    .open = NULL,
    .close = NULL,
};

static void ata_init(void) {
  ata_initialize();
  dev::register_driver("ata", BLOCK_DRIVER, MAJOR_ATA, &ata_ops);
}

module_init("ata", ata_init);
