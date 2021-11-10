#include <dev/disk.h>
#include <dev/driver.h>
#include <dev/mbr.h>
#include <device_majors.h>
#include <errno.h>
#include <printk.h>
#include <module.h>
#include <util.h>


// #include "../../drivers/ata/ata.h"

dev::Disk::Disk() {}
dev::Disk::~Disk(void) {}


dev::DiskPartition::DiskPartition(dev::Disk* parent, u32 start, u32 len) : start(start), len(len) { this->parent = parent; }
dev::DiskPartition::~DiskPartition() {}

bool dev::DiskPartition::read_blocks(uint32_t sector, void* data, int n) {
  if (sector > len) return false;
  // auto *d = (dev::ata*)parent;
  return parent->read_blocks(sector + start, data, n);
}

bool dev::DiskPartition::write_blocks(uint32_t sector, const void* data, int n) {
  if (sector > len) return false;
  return parent->write_blocks(sector + start, data, n);
}

size_t dev::DiskPartition::block_size(void) { return parent->block_size(); }

size_t dev::DiskPartition::block_count(void) { return len; }



static ck::vec<dev::Disk*> m_disks;
static bool initialized = false;


static dev::Disk* get_disk(int minor) {
  if (minor >= 0 && minor < m_disks.size()) {
    return m_disks[minor];
  }
  printk("inval\n");
  return nullptr;
}


static int disk_dev_init(fs::BlockDevice& d) {
  auto disk = get_disk(d.dev.minor());

  d.block_count = disk->block_count();
  d.block_size = disk->block_size();

  return 0;
}


static int disk_rw_block(fs::BlockDevice& b, void* data, int block, bool write) {
  auto d = get_disk(b.dev.minor());
  if (d == NULL) return -1;

  bool success = false;
  if (write) {
    success = d->write_blocks(block, data, 1);
  } else {
    success = d->read_blocks(block, data, 1);
  }
  return success ? 0 : -1;
}

struct fs::BlockOperations generic_disk_blk_ops = {
    .init = disk_dev_init,
    .rw_block = disk_rw_block,
};

static struct dev::DriverInfo disk_driver_info {
  .name = "generic disk", .type = DRIVER_BLOCK, .major = MAJOR_DISK,

  .block_ops = &generic_disk_blk_ops,
};

static int add_drive(const ck::string& name, dev::Disk* drive) {
  printk(KERN_INFO "Add drive '%s'\n", name.get());
  int minor = m_disks.size();
  // KINFO("Detected ATA drive '%s'\n", name.get(), MAJOR_ATA);
  m_disks.push(drive);

  dev::register_name(disk_driver_info, name, minor);
  return minor;
}

int dev::register_disk(dev::Disk* disk) {
  if (!initialized) {
    if (dev::register_driver(disk_driver_info) != 0) {
      panic("failed to register generic disk driver\n");
    }
    initialized = true;
  }


  int minor = m_disks.size();
  ck::string name = ck::string::format("disk%d", minor);
  add_drive(name, disk);


  /* Try to get mbr partitions */
  void* first_sector = malloc(disk->block_size());
  disk->read_blocks(0, (u8*)first_sector, 1);

  if (dev::mbr mbr; mbr.parse(first_sector)) {
    for (int i = 0; i < mbr.part_count(); i++) {
      auto part = mbr.partition(i);
      auto pname = ck::string::format("%sp%d", name.get(), i + 1);

      auto part_disk = new dev::DiskPartition(disk, part.off, part.len);

      add_drive(pname, part_disk);
    }
  }



  free(first_sector);

  return minor;
}


ksh_def("disks", "display all disks") {
	for (auto disk : m_disks) {

		printk("sz: %zu, count: %zu\n", disk->block_size(), disk->block_count());
	}
	return 0;
}
