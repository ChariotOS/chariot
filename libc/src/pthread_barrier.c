#include <pthread.h>
#include <atomic.h>
#include <limits.h>
#include <chariot/futex.h>
#include <sys/sysbind.h>
#include <errno.h>


#define unlikely(c) __builtin_expect((c), 0)

#define BARRIER_IN_THRESHOLD (UINT_MAX / 2)

static int futex_wait(int* ptr, int value) { return sysbind_futex(ptr, FUTEX_WAIT, value, 0, 0, 0); }
static int futex_wake(int* ptr, int value) { return sysbind_futex(ptr, FUTEX_WAKE, value, 0, 0, 0); }

int pthread_barrier_init(pthread_barrier_t* barrier, const pthread_barrierattr_t* attr, unsigned int count) {
  /* XXX EINVAL is not specified by POSIX as a possible error code for COUNT
     being too large.  See pthread_barrier_wait for the reason for the
     comparison with BARRIER_IN_THRESHOLD.  */
  if (unlikely(count == 0 || count >= BARRIER_IN_THRESHOLD)) return EINVAL;

  // const struct pthread_barrierattr* iattr = (attr != NULL ? (struct pthread_barrierattr*)attr : &default_barrierattr);


  /* Initialize the individual fields.  */
  barrier->in = 0;
  barrier->out = 0;
  barrier->count = count;
  barrier->current_round = 0;
  // ibarrier->shared = 0; // (iattr->pshared == PTHREAD_PROCESS_PRIVATE ? FUTEX_PRIVATE : FUTEX_SHARED);

  return 0;
}




int pthread_barrier_wait(pthread_barrier_t* bar) {
  /* How many threads entered so far, including ourself.  */
  unsigned int i;

reset_restart:
  /* Try to enter the barrier.  We need acquire MO to (1) ensure that if we
     observe that our round can be completed (see below for our attempt to do
     so), all pre-barrier-entry effects of all threads in our round happen
     before us completing the round, and (2) to make our use of the barrier
     happen after a potential reset.  We need release MO to make sure that our
     pre-barrier-entry effects happen before threads in this round leaving the
     barrier.  */
  i = atomic_fetch_add_acq_rel(&bar->in, 1) + 1;
  /* These loads are after the fetch_add so that we're less likely to first
     pull in the cache line as shared.  */
  unsigned int count = bar->count;
  /* This is the number of threads that can enter before we need to reset.
     Always at the end of a round.  */
  unsigned int max_in_before_reset = count;  // BARRIER_IN_THRESHOLD - BARRIER_IN_THRESHOLD % count;

  if (i > max_in_before_reset) {
    /* We're in a reset round.  Just wait for a reset to finish; do not
       help finishing previous rounds because this could happen
       concurrently with a reset.  */
    while (i > max_in_before_reset) {
      futex_wait(&bar->in, i);
      // futex_wait_simple(&bar->in, i, bar->shared);
      /* Relaxed MO is fine here because we just need an indication for
         when we should retry to enter (which will use acquire MO, see
         above).  */
      i = atomic_load_relaxed(&bar->in);
    }
    goto reset_restart;
  }

  /* Look at the current round.  At this point, we are just interested in
     whether we can complete rounds, based on the information we obtained
     through our acquire-MO load of IN.  Nonetheless, if we notice that
     our round has been completed using this load, we use the acquire-MO
     fence below to make sure that all pre-barrier-entry effects of all
     threads in our round happen before us leaving the barrier.  Therefore,
     relaxed MO is sufficient.  */
  int cr = atomic_load_relaxed(&bar->current_round);

  /* Try to finish previous rounds and/or the current round.  We simply
     consider just our position here and do not try to do the work of threads
     that entered more recently.  */
  while (cr + count <= i) {
    /* Calculate the new current round based on how many threads entered.
       NEWCR must be larger than CR because CR+COUNT ends a round.  */
    int newcr = i - i % count;
    /* Try to complete previous and/or the current round.  We need release
       MO to propagate the happens-before that we observed through reading
       with acquire MO from IN to other threads.  If the CAS fails, it
       is like the relaxed-MO load of CURRENT_ROUND above.  */
    if (atomic_compare_exchange_weak_release(&bar->current_round, &cr, newcr)) {
      /* Update CR with the modification we just did.  */
      cr = newcr;
      /* Wake threads belonging to the rounds we just finished.  We may
         wake more threads than necessary if more than COUNT threads try
         to block concurrently on the barrier, but this is not a typical
         use of barriers.
         Note that we can still access SHARED because we haven't yet
         confirmed to have left the barrier.  */
      futex_wake(&bar->current_round, INT_MAX);
      /* We did as much as we could based on our position.  If we advanced
         the current round to a round sufficient for us, do not wait for
         that to happen and skip the acquire fence (we already
         synchronize-with all other threads in our round through the
         initial acquire MO fetch_add of IN.  */
      if (i <= cr)
        goto ready_to_leave;
      else
        break;
    }
  }

  /* Wait until the current round is more recent than the round we are in.  */
  while (i > cr) {
    /* Wait for the current round to finish.  */
    futex_wait(&bar->current_round, cr);
    /* See the fence below.  */
    cr = atomic_load_relaxed(&bar->current_round);
  }

  /* Our round finished.  Use the acquire MO fence to synchronize-with the
     thread that finished the round, either through the initial load of
     CURRENT_ROUND above or a failed CAS in the loop above.  */
  atomic_thread_fence_acquire();

  /* Now signal that we left.  */
  unsigned int o;
ready_to_leave:
  /* We need release MO here so that our use of the barrier happens before
     reset or memory reuse after pthread_barrier_destroy.  */
  o = atomic_fetch_add_release(&bar->out, 1) + 1;
  if (o == max_in_before_reset) {
    /* Perform a reset if we are the last pre-reset thread leaving.   All
       other threads accessing the barrier are post-reset threads and are
       incrementing or spinning on IN.  Thus, resetting IN as the last step
       of reset ensures that the reset is not concurrent with actual use of
       the barrier.  We need the acquire MO fence so that the reset happens
       after use of the barrier by all earlier pre-reset threads.  */
    atomic_thread_fence_acquire();
    atomic_store_relaxed(&bar->current_round, 0);
    atomic_store_relaxed(&bar->out, 0);
    /* When destroying the barrier, we wait for a reset to happen.  Thus,
       we must load SHARED now so that this happens before the barrier is
       destroyed.  */
    atomic_store_release(&bar->in, 0);
    futex_wake(&bar->in, INT_MAX);
  }

  /* Return a special value for exactly one thread per round.  */
  return i % count == 0 ? PTHREAD_BARRIER_SERIAL_THREAD : 0;
}




int pthread_barrier_destroy(pthread_barrier_t* bar) {
  /* Destroying a barrier is only allowed if no thread is blocked on it.
     Thus, there is no unfinished round, and all modifications to IN will
     have happened before us (either because the calling thread took part
     in the most recent round and thus synchronized-with all other threads
     entering, or the program ensured this through other synchronization).
     We must wait until all threads that entered so far have confirmed that
     they have exited as well.  To get the notification, pretend that we have
     reached the reset threshold.  */
  unsigned int count = bar->count;
  unsigned int max_in_before_reset = BARRIER_IN_THRESHOLD - BARRIER_IN_THRESHOLD % count;
  /* Relaxed MO sufficient because the program must have ensured that all
     modifications happen-before this load (see above).  */
  unsigned int in = atomic_load_relaxed(&bar->in);
  /* Trigger reset.  The required acquire MO is below.  */
  if (atomic_fetch_add_relaxed(&bar->out, max_in_before_reset - in) < in) {
    /* Not all threads confirmed yet that they have exited, so another
       thread will perform a reset.  Wait until that has happened.  */
    while (in != 0) {
      futex_wait(&bar->in, in);
      in = atomic_load_relaxed(&bar->in);
    }
  }
  /* We must ensure that memory reuse happens after all prior use of the
     barrier (specifically, synchronize-with the reset of the barrier or the
     confirmation of threads leaving the barrier).  */
  atomic_thread_fence_acquire();

  return 0;
}
