#pragma once

#include <fifo_buf.h>
#include <util.h>

// a fifo channel for different types in the kernel
template <typename T>
class chan {
  // A fifo buffer based on bytes
  fifo_buf backing;

 public:
  inline chan(void) {}
  inline ~chan(void) {}

  // sit and wait on data to be recv'd
  inline T recv(void) {
    // this feels wrong, but works fine
    char buf[sizeof(T)];
    int n = backing.read(buf, sizeof(T), true);

    // not sure when this would happen
    if (n != sizeof(T))
      panic("channel read %d bytes when it expected %d", n, sizeof(T));
    return *(T *)buf;
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
};
