#include <dev/disk.h>
#include <dev/driver.h>
#include <dev/mbr.h>
#include <device_majors.h>
#include <errno.h>
#include <printf.h>
#include <module.h>
#include <util.h>
#include <time.h>


dev::Disk::~Disk(void) {}


dev::DiskPartition::DiskPartition(dev::Disk* parent, u32 start, u32 len) : dev::Disk(parent->driver()), start(start), len(len) {
  this->parent = parent;

  set_block_count(len);
  set_block_size(parent->block_size());
  set_size(block_count() * block_size());
}
dev::DiskPartition::~DiskPartition() {}

int dev::DiskPartition::read_blocks(uint32_t block, void* data, int n) {
  if (block > len) return false;
  return parent->read_blocks(block + start, data, n);
}


int dev::DiskPartition::write_blocks(uint32_t block, const void* data, int n) {
  if (block > len) return false;
  return parent->write_blocks(block + start, data, n);
}


static bool initialized = false;

static void add_drive(const ck::string& name, dev::Disk* drive) {
  printf(KERN_INFO "Add drive '%s'. %zd, %zd byte, sectors\n", name.get(), drive->block_count(), drive->block_size());
  drive->bind(name);
}

static int next_disk_id = 0;

int dev::register_disk(dev::Disk* disk) {
  ck::string name = ck::string::format("disk%d", next_disk_id++);
  add_drive(name, disk);


  /* Try to get mbr partitions */
  void* first_sector = malloc(disk->block_size());
  disk->read_block((u8*)first_sector, 0);

  if (dev::mbr mbr; mbr.parse(first_sector)) {
    for (int i = 0; i < mbr.part_count(); i++) {
      auto part = mbr.partition(i);
      auto pname = ck::string::format("%sp%d", name.get(), i + 1);
      printf("Found partition %d, %d\n", part.off, part.len);

      auto part_disk = new dev::DiskPartition(disk, part.off, part.len);
      add_drive(pname, part_disk);
    }
  }


  free(first_sector);
  return 0;
}

/*
ksh_def("disks", "display all disks") {
  for (auto disk : m_disks) {
    printf("sz: %zu, count: %zu\n", disk->block_size(), disk->block_count());
  }
  return 0;
}
*/
