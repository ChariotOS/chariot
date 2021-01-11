#include <asm.h>
#include <dev/mbr.h>
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

dev::mbr::~mbr() {
  if (hdr != nullptr) {
    free(hdr);
  }
}

bool dev::mbr::parse(void *data) {
  hdr = new mbr_header;

	memcpy(hdr, data, sizeof(*hdr));
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

struct dev::part_info dev::mbr::partition(u32 index) {
  assert(index < part_count());
	return m_partitions[index];
}
