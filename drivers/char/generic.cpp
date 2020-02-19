#include <dev/char_dev.h>
#include <dev/driver.h>
#include <errno.h>
#include <mem.h>
#include <module.h>
#include <printk.h>

#include "../majors.h"


#define MINOR_RANDOM 2

static uint32_t seed;  // The state can be seeded with any value.
// Call next() to get 32 pseudo-random bits, call it again to get more bits.
// It may help to make this inline, but you should see if it benefits your
// code.
static inline uint64_t next_random(void) {
  uint32_t z = (seed += 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}

static size_t read_random(char *m, size_t len) {
#define DO_COPY(T) \
  for (; i < len - sizeof(T); i += sizeof(T)) *(T *)(m + i) = next_random();
  int i = 0;
  DO_COPY(u32);
  for (; i < len; i++) *(m + i) = next_random();
#undef DO_COPY
  return len;
}

static ssize_t do_read(fs::filedesc &fd, char *buf, size_t sz) {
  if (fd) {
    switch (fd.ino->minor) {
      case MINOR_RANDOM:
        return read_random(buf, sz);
      default:
        return -1;
    }
  }
  return -1;
}

static ssize_t do_write(fs::filedesc &fd, const char *buf, size_t sz) {
  return -1;
}

static struct dev::driver_ops generic_ops = {
    .llseek = NULL,
    .read = do_read,
    .write = do_write,
    .ioctl = NULL,
    .open = NULL,
    .close = NULL,
};

static void dev_init(void) {
  dev::register_driver("generic", CHAR_DRIVER, MAJOR_MEM, &generic_ops);

  dev::register_name("urandom", MAJOR_ATA, MINOR_RANDOM);
  dev::register_name("random", MAJOR_ATA, MINOR_RANDOM);
}

module_init("char", dev_init);
