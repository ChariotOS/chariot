#include <dev/driver.h>
#include <dev/partition.h>

dev::partition::partition(dev::disk &disk, u32 offset, u32 len)
    : m_disk(disk), m_offset(offset), m_len(len) {
		}

dev::partition::~partition(void) {}

size_t dev::partition::block_size(void) { return m_disk.block_size(); }
size_t dev::partition::block_count(void) { return m_len / block_size(); }

bool dev::partition::read_blocks(u32 index, u8 *buf, int n) {
  return m_disk.read_blocks(index + m_offset, buf, n);
}

bool dev::partition::write_blocks(u32 index, const u8 *buf, int n) {
  return m_disk.write_blocks(index + m_offset, buf, n);
}

