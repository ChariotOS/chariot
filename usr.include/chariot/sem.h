#pragma once

#include <lock.h>
#include <wait.h>

class condvar final {
  unsigned nwaiters = 0;
  unsigned long long wakeup_seq = 0;
  unsigned long long woken_seq = 0;
  unsigned long long main_seq = 0;
  unsigned bcast_seq = 0;

  waitqueue wq;
  spinlock lock;

 public:
  // returns if it successfully waited
  inline bool WARN_UNUSED wait(spinlock &l, bool interruptable = true) {
    // lock the condvar
    lock.lock();
    // release the lock and go to sleep
    l.unlock();

    nwaiters++;
    main_seq++;

    unsigned long long val;
    unsigned long long seq;
    unsigned bc = *(volatile unsigned *)&(bcast_seq);
    val = seq = wakeup_seq;

    do {
      lock.unlock();
      if (interruptable) {
        if (wq.wait() == false /* interrupted? */) {
          // re-take the lock
          lock.lock();
          return false;
        }
      } else {
        wq.wait_noint();
      }
      lock.lock();
      if (bc != *(volatile unsigned *)&(bcast_seq)) goto bcout;
      val = *(volatile unsigned long long *)&(wakeup_seq);
    } while (val == seq || val == *(volatile unsigned long long *)&(woken_seq));

    woken_seq++;
  bcout:
    nwaiters--;
    /* unlock condvar */
    lock.unlock();
    /* reacquire provided lock */
    l.lock();
    return true;
  }

  inline void signal(void) {
    lock.lock();
    // do we have anyone to signal?
    if (main_seq > wakeup_seq) {
      wakeup_seq++;
      wq.notify();
    }
    lock.unlock();
  }

  inline void bcast(void) {
    lock.lock();
    // do we have anyone to wakeup?
    if (main_seq > wakeup_seq) {
      woken_seq = main_seq;
      wakeup_seq = main_seq;
      bcast_seq++;
      lock.unlock();
      wq.notify_all();
    }
    lock.unlock();
  }
};


class semaphore final {
  int value;
  condvar cond;
  spinlock lock;

 public:
  inline semaphore(int value) : value(value) {}

  // returns if it was interrupted or not
  inline bool WARN_UNUSED wait(bool interruptable = true) {
    lock.lock();
    while (value <= 0) {
      if (cond.wait(lock, interruptable) == false /* Interrupted? */) {
        lock.unlock();
        return false;
      }
    }
    value--;
    lock.unlock();
    return true;
  }

  inline void post(void) {
    lock.lock();
    value++;
    cond.signal();
    lock.unlock();
  }
};
