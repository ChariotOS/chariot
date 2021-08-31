#pragma once


#include <pthread.h>
#include <ck/futex.h>

namespace ck {


  class mutex {
   private:
    int m_mutex = 0;

   public:
    mutex(void);
    ~mutex(void);

    void lock(void);
    void unlock(void);
  };


  class scoped_lock {
    ck::mutex &lck;

   public:
    inline scoped_lock(ck::mutex &lck) : lck(lck) {
      lck.lock();
    }

    inline ~scoped_lock(void) {
      lck.unlock();
    }
  };
}  // namespace ck
