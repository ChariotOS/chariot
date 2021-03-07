#include <sched.h>
#include <sem.h>


wait_result condvar::wait(spinlock &given_lock, bool interruptable) {
  // lock the condvar
  lock.lock();

  // release the lock and go to sleep
  given_lock.unlock();

  nwaiters++;
  main_seq++;

  unsigned long long val;
  unsigned long long seq;
  unsigned bc = *(volatile unsigned *)&(bcast_seq);
  val = seq = wakeup_seq;

  do {
    {
      struct wait_entry ent;
      if (interruptable) {
        prepare_to_wait(wq, ent, true);
        sched::set_state(PS_INTERRUPTIBLE);
      } else {
        prepare_to_wait(wq, ent, false);
        sched::set_state(PS_UNINTERRUPTIBLE);
      }

      lock.unlock();

      if (wait_result(ent.result).interrupted()) {
        lock.lock();
        /* There are less threads waiting on this condvar now */
        nwaiters--;
        lock.unlock();
        // re-take the lock
        given_lock.lock();
        return wait_result(WAIT_RES_INTR);
      }


      lock.lock();


      finish_wait(wq, ent);
    }
    if (bc != *(volatile unsigned *)&(bcast_seq)) goto bcout;
    val = *(volatile unsigned long long *)&(wakeup_seq);
  } while (val == seq || val == *(volatile unsigned long long *)&(woken_seq));

  woken_seq++;
bcout:
  /* We're done, the number of waiters */
  nwaiters--;
  /* unlock condvar */
  lock.unlock();

  /* reacquire provided lock */
  given_lock.lock();
  return wait_result();
}
