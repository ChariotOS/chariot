#pragma once

#include <ck/object.h>

namespace ck {
  class futex {
    CK_NONCOPYABLE(futex);
    CK_MAKE_NONMOVABLE(futex);

    volatile int uaddr = 0;

   public:
    futex();
    futex(int initial_value);


    void wait_on(int val);

		void set(int val);
  };
}  // namespace ck
