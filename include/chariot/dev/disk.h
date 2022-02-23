#ifndef __BLK_DEV_H__
#define __BLK_DEV_H__

#include <dev/hardware.h>
#include <dev/driver.h>
#include <ck/ptr.h>

namespace dev {
  class Disk : public fs::BlockDeviceNode {
   public:
    Disk();

    virtual ~Disk();
  };


  class DiskPartition : public dev::Disk {
    dev::Disk* parent;

    uint32_t start, len;

   public:
    DiskPartition(dev::Disk* a, u32 start, u32 len);
    virtual ~DiskPartition();
    virtual ssize_t block_size(void);
    virtual ssize_t block_count(void);
    // virtual int rw_block(void *data, int block, fs::Direction dir);
    virtual int read_blocks(uint32_t sector, void* data, int n);
    virtual int write_blocks(uint32_t sector, const void* data, int n);
  };

  /* returns N in diskN on success. -ERRNO on error */
  int register_disk(dev::Disk* disk);
};  // namespace dev

#endif
