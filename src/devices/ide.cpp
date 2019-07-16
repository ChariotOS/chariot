#include <mobo/dev_mgr.h>




static int ide_init(mobo::device_manager *dm) {
  // printf("ide init\n");

  return 0;
}


mobo_device_register("IDE", ide_init);
