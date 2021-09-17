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
    bool locked = false;

   public:
    inline scoped_lock(ck::mutex &lck) : lck(lck) {
      locked = true;
      lck.lock();
    }

    inline ~scoped_lock(void) { unlock(); }

    inline void unlock(void) {
      if (locked) lck.unlock();
    }
  };
}  // namespace ck
