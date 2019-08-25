#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <types.h>

namespace dev {

  class blk_dev;

class device {
 public:
  inline virtual ~device(){};
  inline virtual bool is_blk_device(void) { return false; }
  inline virtual bool is_char_device(void) { return false; }

  blk_dev *to_blk_dev(void);

};

};  // namespace dev

#endif
