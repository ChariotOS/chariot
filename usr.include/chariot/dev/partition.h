#pragma once

#include <dev/disk.h>
#include <ptr.h>

namespace dev {

/**
 * a partition is just a block device that offsets blocks by a fixed number of
 * blocks. These are usually create by, and owned by an MBR partition table
 */
class partition : public dev::disk {
 public:
  partition(dev::disk &disk, u32 offset, u32 len);
  virtual ~partition();
  virtual size_t block_size(void);
	virtual size_t block_count(void);
  virtual bool read_blocks(u32 index, u8* buf, int n = 1);
  virtual bool write_blocks(u32 index, const u8* buf, int n = 1);

 protected:
  dev::disk &m_disk;
  u32 m_offset;
  u32 m_len;
};

};  // namespace dev
