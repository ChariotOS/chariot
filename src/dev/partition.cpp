#include <dev/partition.h>
#include <dev/driver.h>

dev::partition::partition(dev::blk_dev &disk, u32 offset, u32 len)
    : dev::blk_dev(disk.driver()), m_disk(disk), m_offset(offset), m_len(len) {}

dev::partition::~partition(void) {}

u64 dev::partition::block_size(void) { return m_disk.block_size(); }

bool dev::partition::read_block(u32 index, u8 *buf) {
  return m_disk.read_block(index + m_offset, buf);
}

bool dev::partition::write_block(u32 index, const u8 *buf) {
  return m_disk.write_block(index + m_offset, buf);
}

