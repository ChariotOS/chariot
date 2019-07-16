#include <mobo/dev_mgr.h>

static int debug_in(mobo::port_t, void *ptr, int len, void *) {
  printf("%d\n", len);
  printf("in, world\n");
  *(int*)ptr = 0xfefe;
  return 0;
}

static int debug_out(mobo::port_t, void *, int len, void *ptr) {
  return 0;
}

static int debug_dev_init(mobo::device_manager *dm) {
  dm->hook_io(0xfe, debug_in, debug_out);
  return 0;
}

mobo_device_register("debug", debug_dev_init);
