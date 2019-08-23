#pragma once

#include <dev/blk_dev.h>
#include <ptr.h>
#include <vec.h>



struct mbr_header;


namespace dev {

class mbr {
 public:
  mbr(dev::blk_dev &disk);
  virtual ~mbr();

  // check the MBR header and parse disks out of it, returning true if it was
  // successful. False in any other case
  bool parse(void);

  unique_ptr<dev::blk_dev> partition(u32 index);

  u32 part_count(void);

 protected:


  struct part_info {
    u32 off, len;
  };

  struct mbr_header *hdr = nullptr;
  dev::blk_dev &m_disk;
  vec<part_info> m_partitions;
};

};  // namespace dev
