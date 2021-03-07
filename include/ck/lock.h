#pragma once


#include <pthread.h>


namespace ck {


  class mutex {
   private:
    pthread_mutex_t m_mutex;

   public:
    mutex(void);
    ~mutex(void);

    void lock(void);
    void unlock(void);
  };
}  // namespace ck
