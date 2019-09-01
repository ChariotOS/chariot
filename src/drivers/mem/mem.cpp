#include <dev/char_dev.h>
#include <dev/driver.h>
#include <errno.h>
#include <mem.h>
#include <module.h>
#include <printk.h>
#include "../majors.h"
#include <mem.h>

class mem_device : public dev::char_dev {
 public:
  mem_device(ref<dev::driver> dr) : dev::char_dev(dr) {}

  virtual int read(u64 offset, u32 len, void*dst) override {
    if (offset + len >= mem_size()) return -E2BIG;

    // I'm the kernel, so I can read this :)
    void *start = (char*)p2v(NULL) + offset;

    memcpy(dst, start, len);
    return len;
  }
  virtual int write(u64 offset, u32 len, const void*) override {
    return -ENOTIMPL;
  }
};

class memdev_driver : public dev::driver {
 public:
  memdev_driver() {
    the_device = make_ref<mem_device>(ref<memdev_driver>(this));
  }
  virtual ~memdev_driver(){

  };

  ref<dev::device> open(major_t maj, minor_t min, int &err) {
    if (min == 0) {
      err = 0;
      return the_device;
    }
    err = -ENOENT;
    return nullptr;
  }

  virtual const char *name(void) const { return "memdev"; }

 private:
  ref<mem_device> the_device = nullptr;
};

static void memdev_init(void) {
  printk("[MEM] initializing mem device\n");

  auto d = make_ref<memdev_driver>();
  dev::register_driver(MAJOR_MEM, d);
}

module_init("memdev", memdev_init);
