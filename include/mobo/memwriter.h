#ifndef __MOBO_MEMWRITER__
#define __MOBO_MEMWRITER__


#include <mobo/types.h>
#include <string.h>

namespace mobo {

class memwriter {
  char *mem;
  int written = 0;

 public:
  inline memwriter(void *mem) {
    this->mem = (char *)mem;
  }

  template <typename T>
  inline T *write(T val) {
    T *loc = alloc<T>();
    *loc = val;
    return loc;
  }

  template <typename T>
  inline T *alloc(void) {
    T *loc = (T *)(void *)mem;
    mem += sizeof(T);
    written += sizeof(T);

    memset((void*)loc, 0, sizeof(T));
    return loc;
  }

  inline int get_written(void) { return written; }

  template <typename T>
  inline u64 to_guest(char * base, T *addr) {
    return (u64)addr - (u64)base;
  }
};
}  // namespace mobo


#endif
