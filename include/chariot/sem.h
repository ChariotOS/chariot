#pragma once

#include <lock.h>
#include <wait.h>

class condvar final {
  unsigned nwaiters = 0;
  unsigned long long wakeup_seq = 0;
  unsigned long long woken_seq = 0;
  unsigned long long main_seq = 0;
  unsigned bcast_seq = 0;

  struct wait_queue wq;
  spinlock lock;

 public:
  // returns if it successfully waited
  inline wait_result WARN_UNUSED wait(spinlock &l, bool interruptable = true) {
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
        if (wq.wait().interrupted()) {
          // re-take the lock
          lock.lock();
          return wait_result(WAIT_RES_INTR);
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
    return wait_result();
  }

  inline void signal(void) {
    lock.lock();
    // do we have anyone to signal?
    if (main_seq > wakeup_seq) {
      wakeup_seq++;
      wq.wake_up();
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
      wq.wake_up_all();
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
  inline wait_result WARN_UNUSED wait(bool interruptable = true) {
    lock.lock();
    while (value <= 0) {
      if (cond.wait(lock, interruptable).interrupted()) {
        lock.unlock();
				return wait_result(WAIT_RES_INTR);
        return false;
      }
    }
    value--;
    lock.unlock();
		return wait_result();
  }

  inline void post(void) {
    lock.lock();
    value++;
    cond.signal();
    lock.unlock();
  }
};
