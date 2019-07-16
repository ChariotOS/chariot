#include <mobo/dev_mgr.h>


struct debug_info {
  size_t timer_start = 0;
};


static int debug_dev_init(mobo::device_manager *dm) {
  auto d = new debug_info();



  return 0;
}

mobo_device_register("debug", debug_dev_init);
