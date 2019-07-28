#ifndef __MOBO_MEMWRITER__
#define __MOBO_MEMWRITER__

namespace mobo {

class memwriter {
  char* mem;
  int written = 0;

 public:
  inline memwriter(void* mem) { this->mem = (char*)mem; }

  template<typename T>
    inline void write(T val) {
      *(T*)(void*)mem = val;
      mem += sizeof(T);
      written += sizeof(T);
    }

  inline int get_written(void) {
    return written;
  }
};
}  // namespace mobo

#endif
