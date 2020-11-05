#pragma once

#include <stdlib.h>

namespace ck {
  class resource {
   public:
    static void *get(const char *name, size_t &len);
  };
}  // namespace ck
