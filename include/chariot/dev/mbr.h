#pragma once

#include <dev/disk.h>
#include <ck/ptr.h>
#include <ck/vec.h>

struct mbr_header;

namespace dev {

  struct part_info {
    u32 off, len;
  };

  class mbr {
   public:
    virtual ~mbr();

    // check the MBR header and parse disks out of it, returning true if it was
    // successful. False in any other case
    bool parse(void *);

    struct part_info partition(u32 index);

    u32 part_count(void);

   protected:
    struct mbr_header *hdr = nullptr;
    ck::vec<part_info> m_partitions;
  };

};  // namespace dev
