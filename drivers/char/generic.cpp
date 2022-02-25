#include <dev/char_dev.h>
#include <dev/driver.h>
#include <errno.h>
#include <mem.h>
#include <module.h>
#include <printk.h>

#include "../majors.h"


#define MINOR_RANDOM 2


struct RandomNode : public fs::CharDeviceNode {
	using fs::CharDeviceNode::CharDeviceNode;
  virtual ssize_t read(fs::File &, char *buf, size_t sz) override;
};


static uint32_t seed;  // The state can be seeded with any value.
// Call next() to get 32 pseudo-random bits, call it again to get more bits.
// It may help to make this inline, but you should see if it benefits your
// code.
static inline uint64_t next_random(void) {
  uint64_t z = (seed += 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}

ssize_t RandomNode::read(fs::File &f, char *buf, size_t len) {
#define DO_COPY(T)                            \
  for (; i < len - sizeof(T); i += sizeof(T)) \
    *(T *)(buf + i) = next_random();
  int i = 0;
  DO_COPY(u64);
  for (; i < len; i++)
    *(buf + i) = next_random();
#undef DO_COPY
  return len;
}



static RandomNode random;
static RandomNode urandom;


static void dev_init(void) {
	random.bind("urandom");
	random.bind("random");
}

module_init("char", dev_init);
