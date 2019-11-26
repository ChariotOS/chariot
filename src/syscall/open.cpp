#include <process.h>


int sys::open(const char *path, int flags, int mode) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}
