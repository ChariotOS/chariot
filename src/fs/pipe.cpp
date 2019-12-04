#include <errno.h>
#include <fs.h>

using namespace fs;

fs::pipe::pipe() : fs::inode(T_FIFO) {}

fs::pipe::~pipe(void) {}

ssize_t fs::pipe::do_read(filedesc &fd, void *vbuf, size_t size) {
  ssize_t ret = 0;
  auto *buf = (uint8_t *)vbuf;

  int i = 0;

  lock.lock();

  if (readers == 0) {
    // TODO:
    ret = -EPIPE;
    goto out;
  }

  if (size == 0) {
    ret = -1;
    goto out;
  }

  // if the buffer doesn't have space, wait on it!
  if (used_bytes < size) {
    lock.unlock();
    wq.wait(size);
    lock.lock();
  }

  i = 0;
  // TODO: memcpy!
  while (used_bytes > 0 && i < size) {
    buf[i++] = data[read_ind];
    used_bytes--;
    read_ind++;
    read_ind %= capacity;
  }

  return size;

out:
  lock.unlock();
  return ret;
}

ssize_t fs::pipe::do_write(filedesc &fd, void *vbuf, size_t size) {
  ssize_t ret = 0;
  auto *buf = (uint8_t *)vbuf;

  int i = 0;

  uint32_t bytes_avail = capacity - used_bytes;

  lock.lock();

  if (readers == 0) {
    // TODO:
    ret = -EPIPE;
    goto out;
  }

  if (size == 0) {
    ret = -1;
    goto out;
  }

  if (size > bytes_avail) {
    return -1;
  }

  i = 0;

  while (used_bytes < capacity && i < size) {
    data[write_ind] = buf[i++];
    used_bytes++;
    write_ind++;
    write_ind %= capacity;
  }


  // notify a reader if there is one
  if (wq.should_notify(used_bytes)) {
    wq.notify();
  }

  ret = size;

out:
  lock.unlock();
  return ret;
}
