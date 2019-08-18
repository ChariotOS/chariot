#pragma once

#include <dev/blk_dev.h>
#include <ptr.h>
#include <vec.h>



struct mbr_header;


namespace dev {

class mbr {
 public:
  mbr(ref<dev::blk_dev> disk);
  virtual ~mbr();

  // check the MBR header and parse disks out of it, returning true if it was
  // successful. False in any other case
  bool parse(void);

  ref<dev::blk_dev> partition(u32 index);

  u32 part_count(void);

 protected:
  struct mbr_header *hdr;
  ref<dev::blk_dev> m_disk;
  vec<ref<dev::blk_dev>> m_partitions;
};

};  // namespace dev
