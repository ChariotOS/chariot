#include <dev/driver.h>
#include <module.h>
#include <printk.h>
#include "../majors.h"
#include <mem.h>



class ata_driver : public dev::driver {
 public:
  ata_driver(){
    printk("[ATA] hello, world\n");
  }
  virtual ~ata_driver() {
  }

  virtual const char *name(void) const {
    return "ATA";
  }
};

static void ata_init(void) {

  auto d = make_ref<ata_driver>();


  int rc = dev::register_driver(MAJOR_ATA, d);
  printk("rc=%d\n", rc);
}

module_init("ata", ata_init);
