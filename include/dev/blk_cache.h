#pragma once

#include <dev/blk_dev.h>
#include <mem.h>

namespace dev {
class blk_cache : public dev::blk_dev {
 public:
  blk_cache(dev::blk_dev& disk, u32 cache_size);
  virtual ~blk_cache();
  virtual u64 block_size(void);
  virtual bool read_block(u32 index, u8* buf);
  virtual bool write_block(u32 index, const u8* buf);

 protected:


  void evict(u32 cache_ind);

  struct cache_line {
    bool dirty = false;
    bool valid = false;
    u32 sector = -1;
    void* data = nullptr;
    cache_line(void);
    ~cache_line();
  };


  dev::blk_dev& m_disk;
  u32 m_cache_size;

  cache_line* lines = nullptr;
};
};  // namespace dev
