#pragma once

#include <stdlib.h>
#include <ck/pair.h>
#include <ck/option.h>


namespace ck {
  class resource {
   public:
    static ck::option<ck::pair<void *, size_t>> get(const char *name);
  };
}  // namespace ck
