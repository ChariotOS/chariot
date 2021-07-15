#ifndef __BLK_DEV_H__
#define __BLK_DEV_H__

#include <dev/device.h>
#include <dev/driver.h>
#include <ck/ptr.h>

namespace dev {
  class disk {
   public:
    uint32_t magic = 0xFEEDD00D;

   public:
    disk();

    virtual ~disk();

    virtual size_t block_size() = 0;
    virtual size_t block_count() = 0;

    // all block devices must implement these functions
    virtual bool read_blocks(uint32_t index, void* buf, int n = 1) = 0;
    virtual bool write_blocks(uint32_t index, const void* buf, int n = 1) = 0;
  };


  class disk_part : public dev::disk {
    dev::disk* parent;

    uint32_t start, len;

   public:
    disk_part(dev::disk* a, u32 start, u32 len);
    virtual ~disk_part();
    virtual bool read_blocks(uint32_t sector, void* data, int n = 1);
    virtual bool write_blocks(uint32_t sector, const void* data, int n = 1);
    virtual size_t block_size(void);
    virtual size_t block_count(void);
  };

  /* returns N in diskN on success. -ERRNO on error */
  int register_disk(dev::disk* disk);
};  // namespace dev

#endif
