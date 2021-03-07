#pragma once

#include <awaitfs.h>
#include <fifo_buf.h>
#include <util.h>
#include <fs.h>
#include <errno.h>

// a fifo channel for different types in the kernel
template <typename T>
class chan {
  // A fifo buffer based on bytes
  fifo_buf backing;

 public:
  inline chan(void) {
  }
  inline ~chan(void) {
  }

  // sit and wait on data to be recv'd
  inline T recv(void) {
    // this feels wrong, but works fine
    char buf[sizeof(T)];
    int n = backing.read(buf, sizeof(T), true);

    // not sure when this would happen
    if (n != sizeof(T)) panic("channel read %d bytes when it expected %d", n, sizeof(T));
    T val = *(T *)(void *)buf;
    return val;
  }


  // sit and wait on data to be recv'd
  inline int recv_timeout(long long timeout, T *dst) {
    // this feels wrong, but works fine
    char buf[sizeof(T)];
    int n = backing.read(buf, sizeof(T), true, timeout);
    if (n == -ETIMEDOUT) {
      return -ETIMEDOUT;
    }

    // not sure when this would happen
    if (n != sizeof(T)) panic("channel read %d bytes when it expected %d", n, sizeof(T));
    *dst = *(T *)(void *)buf;
    return 0;
  }

  inline void send(T &&val, bool wait = false) {
    // HACK: move the thing into the buffer. We do this to avoid calling the
    //       destructor for `val` when this function returns.
    char buf[sizeof(T)];
    *((T *)buf) = move(val);
    backing.write(buf, sizeof(T), wait);
  }

  inline bool avail(void) {
    return backing.size() >= sizeof(T);
  }

  inline int poll(poll_table &pt) {
    int ev = 0;

    ev |= backing.poll(pt);

    if (avail()) ev |= AWAITFS_READ;
    ev |= AWAITFS_WRITE;

    return ev;
  }
};
