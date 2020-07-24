#pragma once

#include <ck/io.h>
#include <stdlib.h>
#include <unistd.h>

namespace ck {


  namespace mem {


		template<typename T>
			class tracked {
			 public:
				inline void *operator new(size_t sz) {
					T *ptr = (T*)malloc(sz);
					ck::out.fmt("[mem::tracked (%d)] alloc %p\n", getpid(), ptr);
					return ptr;
				}


				inline void operator delete(void *ptr) {
					ck::out.fmt("[mem::tracked (%d)] free %p\n", getpid(), ptr);
					// ck::hexdump(ptr, sz);
					free(ptr);
				}
			};




    template <typename T>
    inline T *set(void *vdst, T val, size_t size) {
      T *dst = (T *)vdst;
      for (size_t i = 0; i < size; i++) dst[i] = val;
      return dst;
    }

  }  // namespace mem

};  // namespace ck
