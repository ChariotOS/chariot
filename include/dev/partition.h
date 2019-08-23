#pragma once

#include <dev/blk_dev.h>
#include <ptr.h>

namespace dev {

/**
 * a partition is just a block device that offsets blocks by a fixed number of
 * blocks. These are usually create by, and owned by an MBR partition table
 */
class partition : public dev::blk_dev {
 public:
  partition(dev::blk_dev &disk, u32 offset, u32 len);
  virtual ~partition();
  virtual u64 block_size(void);
  virtual bool read_block(u32 index, u8* buf);
  virtual bool write_block(u32 index, const u8* buf);

 protected:
  dev::blk_dev &m_disk;
  u32 m_offset;
  u32 m_len;
};

};  // namespace dev
