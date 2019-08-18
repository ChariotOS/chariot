#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <types.h>

namespace dev {

class device {
 public:
  inline virtual ~device(){};
  inline virtual bool is_blk_device(void) { return false; }
  inline virtual bool is_char_device(void) { return false; }
};

};  // namespace dev

#endif
