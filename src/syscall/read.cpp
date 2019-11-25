#include <process.h>

ssize_t sys::read(int fd, void *dst, size_t len) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}
