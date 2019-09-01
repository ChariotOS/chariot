#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <types.h>
#include <ptr.h>

namespace dev {

  class blk_dev;
  class char_dev;

  class driver;

class device : public refcounted<device> {
 public:

  device(ref<dev::driver> dr);
  virtual ~device();
  inline virtual bool is_blk_device(void) { return false; }
  inline virtual bool is_char_device(void) { return false; }

  blk_dev *to_blk_dev(void);
  char_dev *to_char_dev(void);


  virtual int read(u64 offset, u32 len, void*) = 0;
  virtual int write(u64 offset, u32 len, const void*) = 0;


  ref<dev::driver> driver();

 private:
  ref<dev::driver> m_driver;

};

};  // namespace dev

#endif
