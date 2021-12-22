#include "ata.h"

#include <module.h>
#include <dev/driver.h>
#include <dev/hardware.h>
#include <util.h>
#include <sleep.h>


static spinlock drive_lock;


#ifdef CONFIG_ATA_DEBUG
#define PIIX_INFO(fmt, args...) KINFO("PIIX: " fmt, ##args)
#else
#define PIIX_INFO(fmt, args...)
#endif


#define ATA_SR_BSY 0x80   // Busy
#define ATA_SR_DRDY 0x40  // Drive ready
#define ATA_SR_DF 0x20    // Drive write fault
#define ATA_SR_DSC 0x10   // Drive seek complete
#define ATA_SR_DRQ 0x08   // Data request ready
#define ATA_SR_CORR 0x04  // Corrected data
#define ATA_SR_IDX 0x02   // Index
#define ATA_SR_ERR 0x01   // Error

#define ATA_ER_BBK 0x80    // Bad block
#define ATA_ER_UNC 0x40    // Uncorrectable data
#define ATA_ER_MC 0x20     // Media changed
#define ATA_ER_IDNF 0x10   // ID mark not found
#define ATA_ER_MCR 0x08    // Media change request
#define ATA_ER_ABRT 0x04   // Command aborted
#define ATA_ER_TK0NF 0x02  // Track 0 not found
#define ATA_ER_AMNF 0x01   // No address mark


#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_READ_PIO_EXT 0x24
#define ATA_CMD_READ_DMA 0xC8
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_WRITE_PIO_EXT 0x34
#define ATA_CMD_WRITE_DMA 0xCA
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_PACKET 0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY 0xEC


#define ATAPI_CMD_READ 0xA8
#define ATAPI_CMD_EJECT 0x1B

#define ATA_IDENT_DEVICETYPE 0
#define ATA_IDENT_CYLINDERS 2
#define ATA_IDENT_HEADS 6
#define ATA_IDENT_SECTORS 12
#define ATA_IDENT_SERIAL 20
#define ATA_IDENT_MODEL 54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID 106
#define ATA_IDENT_MAX_LBA 120
#define ATA_IDENT_COMMANDSETS 164
#define ATA_IDENT_MAX_LBA_EXT 200


#define IDE_ATA 0x00
#define IDE_ATAPI 0x01

#define ATA_MASTER 0x00
#define ATA_SLAVE 0x01



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


typedef union ata_status_reg {
  uint8_t val;
  struct {
    uint8_t err : 1;  // error occurred
    uint8_t rsv1 : 2;
    uint8_t drq : 1;  // drive has data ready or can accept data (PIO)
    uint8_t srv : 1;  // overlapped mode?
    uint8_t df : 1;   // drive fault
    uint8_t rdy : 1;  // drive ready
    uint8_t bsy : 1;  // drive busy
  };
} __packed ata_status_reg_t;

typedef union ata_cmd_reg {
  uint8_t val;
  struct {
    uint8_t rsvd1 : 1;
    uint8_t ien : 1;   // interrupt enable - ACTIVE LOW
    uint8_t srst : 1;  // software reset of all drives on bus
    uint8_t rsv2 : 4;
    uint8_t hob : 1;  // read back high-order byte of LBA48 (?)
  };
} __packed ata_cmd_reg_t;

typedef enum { OK = 0, ERR, DF } ata_error_t;


namespace piix {



  // this is a basic driver, not a block driver.
  // Disks are abstracted by dev::Disk.
  class Driver : public dev::Driver {
   public:
    using dev::Driver::Driver;
    virtual ~Driver(void) {}
    virtual void init(void);
  };

  // A piix disk is an instance of a PIIX PCI disk
  class Disk : public dev::Disk {
   public:
    word_port data_port;
    byte_port error_port;
    byte_port sector_count_port;
    byte_port lba0_port;
    byte_port lba1_port;
    byte_port lba2_port;
    byte_port device_port;
    byte_port command_port;
    byte_port control_port;

    u16 m_io_base;

    struct pdrt {
      u64 offset;
      u16 size{0};
      u16 end_of_table{0};
    };

    bool master;
    u16 sector_size;

    void select_device();


    u8 wait(void);

    u64 n_sectors = 0;

    bool use_dma;
    // where DMA will take place
    void* m_dma_buffer = nullptr;

    bool is_partition = false;

    pci::device* m_pci_dev;
    u16 bmr_command;
    u16 bmr_status;
    u16 bmr_prdt;

    uint16_t id_buf[1024] = {0};

    struct prdt_t {
      uint32_t buffer_phys;
      uint16_t transfer_size;
      uint16_t mark_end;
    } __attribute__((packed));


    ck::ref<hw::PCIDevice> dev;

    Disk(ck::ref<hw::PCIDevice> dev, int bar0, int bar1, bool primary);
    virtual ~Disk(void) {}

    virtual bool read_blocks(uint32_t sector, void* data, int n);
    virtual bool write_blocks(uint32_t sector, const void* data, int n);
    virtual size_t block_size(void);
    virtual size_t block_count(void);


    // wait on the status for a certain value
    ata_error_t wait(int val);

    bool identify();
  };

}  // namespace piix



piix::Disk::Disk(ck::ref<hw::PCIDevice> dev, int b0, int b1, bool primary) : dev(dev) {
  scoped_lock l(drive_lock);
  auto bar0 = dev->get_bar(b0).raw;
  auto bar1 = dev->get_bar(b1).raw;

  m_io_base = bar0;
  // we dont know about this
  sector_size = 512;
  this->master = primary;

  data_port = bar0;
  error_port = bar0 + 1;
  sector_count_port = bar0 + 2;
  lba0_port = bar0 + 3;
  lba1_port = bar0 + 4;
  lba2_port = bar0 + 5;
  device_port = bar0 + 6;
  command_port = bar0 + 7;
  control_port = bar1 + 2;
}



void piix::Disk::select_device() { device_port.out(master ? 0xA0 : 0xB0); }

bool piix::Disk::read_blocks(uint32_t sector, void* data, int n) { return false; }
bool piix::Disk::write_blocks(uint32_t sector, const void* data, int n) { return false; }
size_t piix::Disk::block_size(void) { return 0; }
size_t piix::Disk::block_count(void) { return 0; }



ata_error_t piix::Disk::wait(int data) {
  PIIX_INFO("Waiting on drive for %s\n", data ? "data" : "command");

  ata_status_reg_t stat;
  while (1) {
    stat.val = command_port.in();
    if (stat.err) {
      KERR("Controller error (0x%x)\n", stat.val);
      return ERR;
    }
    if (stat.df) {
      KERR("Drive fault (0x%x)\n", stat.val);
      return DF;
    }
    if (!stat.bsy && (!data || stat.drq)) {
      KERR("Leaving wait with status=0x%x\n", stat.val);
      return OK;
    }
  }
}

bool piix::Disk::identify(void) {
  scoped_irqlock l(drive_lock);

  uint8_t status = 0;
  uint8_t err = 0;

  int type = 0;

  // select the correct device
  select_device();

  sector_count_port.out(0);
  lba0_port.out(0);
  lba1_port.out(0);
  lba2_port.out(0);
  command_port.out(ATA_CMD_IDENTIFY);

  // TODO: should this be negated?
  if (command_port.in()) {
    // nonexistent drive... why am I identifying it?
    PIIX_INFO("Drive is nonexistent\n");
    return false;
  }

  PIIX_INFO("Drive exists\n");

  // hexdump(id_buf, 512, true);

  return false;
}

static void identify(piix::Disk* disk) {
  if (disk->identify()) {
    dev::register_disk(disk);
  } else {
    delete disk;
  }
}

static dev::ProbeResult piix_probe(ck::ref<hw::Device> dev) {
  if (auto pci = dev->cast<hw::PCIDevice>()) {
    if (pci->class_id == 0x01 && pci->subclass_id == 0x01) {
      PIIX_INFO("Found ATA piix device: %s\n", pci->name().get());


      return dev::ProbeResult::Attach;
    }
  }

  return dev::ProbeResult::Ignore;
}



void piix::Driver::init(void) {
  if (auto pci = dev()->cast<hw::PCIDevice>()) {
    uint8_t pif = pci->read8(0x9);  // prog interface
    PIIX_INFO("Programming interface: %x\n", pif);



    if (pif & (1 << 0)) PIIX_INFO("Primary Channel in PCI Mode\n");
    if (pif & (1 << 1)) PIIX_INFO("Primary Channel can be switched to PCI Mode\n");
    if (pif & (1 << 2)) PIIX_INFO("Secondary Channel in PCI Mode\n");
    if (pif & (1 << 3)) PIIX_INFO("Secondary Channel can be switched to PCI Mode\n");
    if (pif & (1 << 7)) PIIX_INFO("Supports DMA\n");
    PIIX_INFO("Interrupt: %d\n", pci->interrupt);
    for (int i = 0; i < 5; i++) {
      PIIX_INFO("BAR%d = %p\n", i, pci->get_bar(i).raw);
    }

    // Primary channel in PCI mode
    if (pif & (1 << 0)) {
      identify(new piix::Disk(pci, 0, 1, true));
      identify(new piix::Disk(pci, 0, 1, false));
    }

    // secondary channel in PCI mode
    if (pif & (1 << 2)) {
      identify(new piix::Disk(pci, 2, 3, true));
      identify(new piix::Disk(pci, 2, 3, false));
    }
  }
}

driver_init("ata-piix", piix::Driver, piix_probe);

