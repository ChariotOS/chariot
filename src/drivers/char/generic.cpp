#include <dev/char_dev.h>
#include <dev/driver.h>
#include <errno.h>
#include <mem.h>
#include <module.h>
#include <printk.h>
#include "../majors.h"

class mem_device : public dev::char_dev {
 public:
  mem_device(ref<dev::driver> dr) : dev::char_dev(dr) {}

  virtual int read(u64 offset, u32 len, void *dst) override {
    if (offset + len >= mem_size()) return -E2BIG;

    // I'm the kernel, so I can read this :)
    void *start = (char *)p2v(NULL) + offset;

    memcpy(dst, start, len);
    return len;
  }
  virtual int write(u64 offset, u32 len, const void *) override {
    return -ENOTIMPL;
  }
};

class null_device : public dev::char_dev {
 public:
  null_device(ref<dev::driver> dr) : dev::char_dev(dr) {}

  /**
   * the null device simply erases the buffer with nulls
   */
  virtual int read(u64 offset, u32 len, void *dst) override {
    memset(dst, 0, len);
    return len;
  }

  /**
   * Writing to null simply accepts the data, without using it anywhere
   */
  virtual int write(u64 offset, u32 len, const void *) override { return len; }
};

class random_device : public dev::char_dev {
  uint32_t x;  // The state can be seeded with any value.
  // Call next() to get 32 pseudo-random bits, call it again to get more bits.
  // It may help to make this inline, but you should see if it benefits your
  // code.
  inline uint32_t next(void) {
    uint32_t z = (x += 0x6D2B79F5UL);
    z = (z ^ (z >> 15)) * (z | 1UL);
    z ^= z + (z ^ (z >> 7)) * (z | 61UL);
    return z ^ (z >> 14);
  }

 public:
  random_device(ref<dev::driver> dr) : dev::char_dev(dr) {}

  virtual int read(u64 offset, u32 len, void *dst) override {
    u8 *p = (u8 *)dst;
    // TODO: Optimize by reading u32s into the buffer until there isnt space,
    // then reading bytes
    for_range(i, 0, len) { p[i] = next(); }
    return len;
  }

  virtual int write(u64 offset, u32 len, const void *) override {
    // TODO: use the data to seed or something
    return len;
  }
};

class one_device : public dev::char_dev {
 public:
  one_device(ref<dev::driver> dr) : dev::char_dev(dr) {}
  virtual int read(u64 offset, u32 len, void *dst) override {
    memset(dst, 0xFF, len);
    return len;
  }
  virtual int write(u64 offset, u32 len, const void *) override { return len; }
};

class chardev_driver : public dev::driver {
 public:
  chardev_driver() {
    // register the memory device on minor 0
    m_mem = make_ref<mem_device>(ref<chardev_driver>(this));
    dev::register_name("mem", MAJOR_MEM, 0); // TODO: register this as ROOT ONLY

    // register random device on minor 1
    m_null = make_ref<null_device>(ref<chardev_driver>(this));
    dev::register_name("null", MAJOR_MEM, 1);

    // register random device on minor 2
    m_random = make_ref<random_device>(ref<chardev_driver>(this));
    dev::register_name("random", MAJOR_MEM, 2);
    dev::register_name("urandom", MAJOR_MEM, 2);

    // register one device on minor 3
    m_one = make_ref<one_device>(ref<chardev_driver>(this));
    dev::register_name("one", MAJOR_MEM, 3);
  }

  virtual ~chardev_driver(){};

  ref<dev::device> open(major_t maj, minor_t min, int &err) {
    if (min == 0) {
      err = 0;
      return m_mem;
    }
    if (min == 1) {
      err = 0;
      return m_null;
    }
    if (min == 2) {
      err = 0;
      return m_random;
    }
    if (min == 3) {
      err = 0;
      return m_one;
    }
    err = -ENOENT;
    return nullptr;
  }

  virtual const char *name(void) const { return "char"; }

 private:
  ref<mem_device> m_mem = nullptr;
  ref<null_device> m_null = nullptr;
  ref<random_device> m_random = nullptr;
  ref<one_device> m_one = nullptr;
};

static void dev_init(void) {
  auto d = make_ref<chardev_driver>();
  dev::register_driver(MAJOR_MEM, d);
}

module_init("char", dev_init);
