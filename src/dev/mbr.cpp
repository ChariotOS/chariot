#include <asm.h>
#include <dev/mbr.h>
#include <dev/partition.h>
#include <mem.h>

#define MBR_SIGNATURE 0xaa55

struct mbr_part_entry {
  u8 status;
  u8 chs1[3];
  u8 type;
  u8 chs2[3];
  u32 offset;
  u32 length;
} __attribute__((packed));

struct mbr_header {
  u8 code1[218];
  u16 ts_zero;
  u8 ts_drive, ts_seconds, ts_minutes, ts_hours;
  u8 code2[216];
  u32 disk_signature;
  u16 disk_signature_zero;
  mbr_part_entry entry[4];
  u16 mbr_signature;
} __attribute__((packed));

dev::mbr::mbr(dev::blk_dev &disk) : m_disk(disk) {}

dev::mbr::~mbr() {
  if (hdr != nullptr) {
    kfree(hdr);
  }
}

bool dev::mbr::parse(void) {
  hdr = (struct mbr_header *)kmalloc(sizeof(mbr_header));

  if (!m_disk.read_block(0, (u8 *)hdr)) {
    return false;
  }

  if (hdr->mbr_signature != MBR_SIGNATURE) {
    // printk("dev::mbr::parse: bad mbr signature %04X\n", hdr->mbr_signature);
    return false;
  }

  for_range(i, 0, 4) {
    auto &entry = hdr->entry[i];

    u32 offset = entry.offset;

    if (offset == 0) continue;
    m_partitions.push({.off = offset, .len = entry.length});
  }

  // parse succeeded
  return true;
}

u32 dev::mbr::part_count(void) { return m_partitions.size(); }

unique_ptr<dev::blk_dev> dev::mbr::partition(u32 index) {
  assert(index < part_count());

  auto info = m_partitions[index];

  return make_unique<dev::partition>(m_disk, info.off, info.len);
}
