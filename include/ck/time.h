#pragma once

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

namespace ck {
  namespace time {
    uint64_t us();
    inline uint64_t ms(void) { return ck::time::us() / 1000; }
    inline uint64_t sec(void) { return ck::time::ms() / 1000; }


    class tracker {
     public:
      inline tracker(void) { restart(); }
      inline auto us(void) { return ck::time::us() - m_start_us; }
      inline auto ms(void) { return this->us() / 1000; }
      inline auto sec(void) { return this->ms() / 1000; }

      inline void restart(void) { m_start_us = ck::time::us(); }
      inline auto start_us(void) { return m_start_us; }
      inline auto start_ms(void) { return start_us() / 1000; }
      inline auto start_sec(void) { return start_ms() / 1000; }

     private:
      uint64_t m_start_us = 0;
    };


    class logger : public tracker {
     public:
      inline logger(const char *msg) : msg(msg) { restart(); }
      ~logger() { printf("[%d] '%s' took %llu us\n", getpid(), msg, us()); }

     private:
      const char *msg = "unknown";
      uint64_t m_start_us = 0;
    };

  }  // namespace time
}  // namespace ck