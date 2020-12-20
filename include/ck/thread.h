#pragma once

#include <pthread.h>
#include <stdlib.h>

namespace ck {

  class thread {
   private:
    /* just backed by pthreads */
    pthread_t thd;
    bool joinable = false;

   public:
    template <class Fn>
    explicit thread(Fn fn) {
      joinable = true;
      struct payload {
        Fn fn;
        payload(Fn &&fn) : fn(fn) {}
      };

      auto *pl = new payload(move(fn));

      pthread_create(
          &thd, NULL,
          [](void *arg) -> void * {
            struct payload *pl = (struct payload *)arg;
            pl->fn();
            delete pl;
            return NULL;
          },
          (void *)pl);
    }


    /* Default constructor */
    inline thread(void) { joinable = false; }

    /* No copy constructor (doesn't make sense) */
    thread(const thread &) = delete;

    ~thread(void);

    /* Move assign */
    thread &operator=(thread &&rhs) noexcept;
    /* Copy assign disallowed */
    thread &operator=(const thread &) = delete;

    void join(void);
  };
}  // namespace ck
