#pragma once

#include <limits.h>
#include <stdint.h>

namespace ck {

  class rand {
    uint64_t seed = 0;

    inline auto tsc(void) {
      uint32_t lo, hi;
      asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
      return lo | ((uint64_t)(hi) << 32);
    }

   public:
    // construct with a "random" seed
    inline rand() { this->seed = tsc(); }

    // construct with a given seed
    inline rand(uint64_t seed) : seed(seed) {}

    inline int next() {
      seed = 6364136223846793005ULL * seed + 1;
      return seed >> 33;
    }

    inline double next_float() { return next() / (double)INT_MAX; }

    inline double uniform(double a, double b) {
      return a + (b - a) * next_float();
    }
  };
}  // namespace ck
