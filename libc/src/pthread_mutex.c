#include <pthread.h>
#include <atomic.h>
#include <limits.h>
#include <chariot/futex.h>
#include <sys/sysbind.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>


#define unlikely(c) __builtin_expect((c), 0)

static int futex_wait(int *ptr, int value) { return sysbind_futex(ptr, FUTEX_WAIT, value, 0, 0, 0); }
static int futex_wake(int *ptr, int value) { return sysbind_futex(ptr, FUTEX_WAKE, value, 0, 0, 0); }


static inline uint32_t cmpxchg32(void *m, uint32_t old, uint32_t newval) {
  return __atomic_compare_exchange_n((uint32_t *)m, &old, newval, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}



int pthread_mutex_lock(pthread_mutex_t *m) {
  // try to atimically swap 0 -> 1
  if (cmpxchg32(&m->word, 0, 1)) return 0;  // success
  // wasn't zero -- somebody held the lock
  do {
    // assume lock is still taken, try to make it 2 and wait
    if (m->word == 2 || cmpxchg32(&m->word, 1, 2)) {
      // let's wait, but only if the value is still 2
      futex_wait(&m->word, 2);
    }
    // try (again) assuming the lock is free
  } while (!cmpxchg32(&m->word, 0, 2));
  // we are here only if transition 0 -> 2 succeeded
  return 0;
}


int pthread_mutex_trylock(pthread_mutex_t *m) {
  // try to atimically swap 0 -> 1
  if (cmpxchg32(&m->word, 0, 1)) return 0;  // success
  return -EBUSY;
}

int pthread_mutex_unlock(pthread_mutex_t *m) {
  int res = __atomic_fetch_sub(&m->word, 1, __ATOMIC_SEQ_CST);
  // we own the lock, so it's either 1 or 2
  if (res != 1) {
    // it was 2
    // important: we are still holding the lock!
    m->word = 0;  // not any more
    // wake up one thread (no fairness assumed)
    futex_wake(&m->word, 1);
  }
	return 0;
}


int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  memset(mutex, 0, sizeof(*mutex));
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) { return 0; }
