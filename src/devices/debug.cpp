#include <mobo/dev_mgr.h>

static int debug_io(bool in, mobo::port_t p, void *data, int sz, void *) {
  char *buf = (char *)data;
  printf("[%s] %4x : (%d) ", in ? "IN " : "OUT", p, sz);
  for (int i = 0; i < sz; i++) {
    printf("%02x ", buf[sz]);
  }
  printf("\n");
  return 0;
}

static int putchar_handle(mobo::port_t, void *data, int sz, void *) {
  if (sz == 0) return 0;

  printf("%c", *(char *)data);
  return 0;
}

static int getchar_handle(mobo::port_t, void *, int, void *) { return 0; }



static inline uint64_t __attribute__((always_inline)) rdtsc(void) {
  uint64_t lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}


static int timing_handle(mobo::port_t p, void *, int, void *) {

  static int count = 0;
  static uint64_t start = 0;
  // set timer
  if (p == 0xff) {
    start = rdtsc();
  }

  // stop timer
  if (p == 0xee) {
    uint64_t now = rdtsc();
    printf("%d, %lu\n", count, now-start);
    if (count++ == 1000) exit(0);
  }



  return 0;
}



static int debug_dev_init(mobo::device_manager *dm) {
  using namespace std::placeholders;  // for _1, _2, _3...
  //                 in              out
  dm->hook_io(0xfad, getchar_handle, putchar_handle);
  dm->hook_io(0x000, std::bind(debug_io, true, _1, _2, _3, _4),
              std::bind(debug_io, false, _1, _2, _3, _4));



  dm->hook_io(0xee, timing_handle, timing_handle);
  dm->hook_io(0xff, timing_handle, timing_handle);


  dm->hook_io(0xff00, std::bind(debug_io, true, _1, _2, _3, _4),
              std::bind(debug_io, false, _1, _2, _3, _4));


  dm->hook_io(0x00db, std::bind(debug_io, true, _1, _2, _3, _4),
              std::bind(debug_io, false, _1, _2, _3, _4));
  return 0;
}

mobo_device_register("debug", debug_dev_init);
