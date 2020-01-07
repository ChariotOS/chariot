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
  virtual ssize_t size(void) override { return mem_size(); }
};

/**
 * null_device - black hole device
 */
class null_device : public dev::char_dev {
 public:
  null_device(ref<dev::driver> dr) : dev::char_dev(dr) {}
  virtual int read(u64 offset, u32 len, void *dst) override { return 0; }
  virtual int write(u64 offset, u32 len, const void *) override { return len; }
  virtual ssize_t size(void) override { return -1; }
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
  random_device(ref<dev::driver> dr) : dev::char_dev(dr) { x = arch::read_timestamp(); }

  virtual int read(u64 offset, u32 len, void *vdst) override {
    // TODO: Optimize by reading u32s into the buffer until there isnt space,
    // then reading bytes

    char *m = (char *)vdst;
#define DO_COPY(T) \
  for (; i < len - sizeof(T); i += sizeof(T)) *(T *)(m + i) = next();
    int i = 0;
    DO_COPY(u32);
    for (; i < len; i++) *(m + i) = next();
#undef DO_COPY
    return len;
  }

  virtual int write(u64 offset, u32 len, const void *) override {
    // TODO: use the data to seed or something
    return len;
  }

  virtual ssize_t size(void) override { return -1; }
};

/**
 * zero_device - just return all zeroes
 */
class zero_device : public dev::char_dev {
 public:
  zero_device(ref<dev::driver> dr) : dev::char_dev(dr) {}
  virtual int read(u64 offset, u32 len, void *dst) override {
    memset(dst, 0, len);
    return len;
  }
  virtual int write(u64 offset, u32 len, const void *) override { return len; }
  virtual ssize_t size(void) override { return -1; }
};

/**
 * one_device - much like null, but returns all ones (0xFF)
 */
class one_device : public dev::char_dev {
 public:
  one_device(ref<dev::driver> dr) : dev::char_dev(dr) {}
  virtual int read(u64 offset, u32 len, void *dst) override {
    memset(dst, 0xFF, len);
    return len;
  }
  virtual int write(u64 offset, u32 len, const void *) override { return len; }
  virtual ssize_t size(void) override { return -1; }
};

class chardev_driver : public dev::driver {
 public:
  chardev_driver() {
    for (int i = 0; i < 8; i++) {
      devices[i] = nullptr;
    }
    // register the memory device on minor 0
    devices[0] = make_ref<mem_device>(ref<chardev_driver>(this));
    dev::register_name("mem", MAJOR_MEM,
                       0);  // TODO: register this as ROOT ONLY

    // register random device on minor 1
    devices[1] = make_ref<null_device>(ref<chardev_driver>(this));
    dev::register_name("null", MAJOR_MEM, 1);

    // register random device on minor 2
    devices[2] = make_ref<random_device>(ref<chardev_driver>(this));
    dev::register_name("random", MAJOR_MEM, 2);
    dev::register_name("urandom", MAJOR_MEM, 2);

    // register one device on minor 3
    devices[3] = make_ref<zero_device>(ref<chardev_driver>(this));
    dev::register_name("zero", MAJOR_MEM, 3);

    // register one device on minor 4
    devices[4] = make_ref<one_device>(ref<chardev_driver>(this));
    dev::register_name("one", MAJOR_MEM, 4);
  }

  virtual ~chardev_driver(){};

  ref<dev::device> open(major_t maj, minor_t min, int &err) {
    if (min > 8 || min < 0) {
      return devices[min];
    }
    err = -ENOENT;
    return nullptr;
  }

  virtual ssize_t read(minor_t min, fs::filedesc &fd, void *buf, size_t sz) {
    if (min > 8 || min < 0) return -1;
    dev::char_dev *d = devices[min].get();
    if (d == NULL) return -1;
    return d->read(0, sz, buf);
  };

  virtual ssize_t write(minor_t min, fs::filedesc &fd, const void *buf,
                        size_t sz) {
    if (min > 8 || min < 0) return -1;
    dev::char_dev *d = devices[min].get();
    if (d == NULL) return -1;
    return d->write(0, sz, buf);
  };

  virtual const char *name(void) const { return "char"; }

 private:
  ref<dev::char_dev> devices[8];
  /*
  ref<mem_device> m_mem = nullptr;
  ref<null_device> m_null = nullptr;
  ref<random_device> m_random = nullptr;
  ref<zero_device> m_zero = nullptr;
  ref<one_device> m_one = nullptr;
  */
};

static void dev_init(void) {
  // dev::register_driver(MAJOR_MEM, make_unique<chardev_driver>());
}

module_init("char", dev_init);
