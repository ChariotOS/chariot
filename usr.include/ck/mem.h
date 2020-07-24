#pragma once

#include <stdlib.h>

namespace ck {




	template<typename T>
		inline T *memset(void *vdst, T val, size_t size) {
			T *dst = (T*)vdst;
			for (size_t i = 0; i < size; i++) dst[i] = val;
			return dst;
		}
};
