#include <dev/char_dev.h>
#include <printk.h>
#include <errno.h>
#include <dev/driver.h>

dev::char_dev::char_dev(ref<dev::driver> dr) : dev::device(dr) {

}

dev::char_dev::~char_dev(void) {}

int dev::char_dev::read(u64 offset, u32 len, void *data) {
  return -ENOTIMPL;
}

int dev::char_dev::write(u64 offset, u32 len, const void *data) {
  return -ENOTIMPL;
}

