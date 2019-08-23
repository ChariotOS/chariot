#include <lock.h>

#define memory_barrier() asm volatile("" ::: "memory")

mutex_lock::mutex_lock(const char *name) : name(name) {
  locked = 0;
  // TODO: use a better cpu state struct (maybe a ptr?)
  holding_cpu = -1;
}

void mutex_lock::lock(void) {
  // TODO: check if interrupts are disabled. Wouldn't want to deadlock on
  // preemption
  while (true) {
    if (compare_and_swap(&locked, 1, 0) == 0) {
      // TODO: set the holding_cpu value, we now have the lock
      locked = 0;
      memory_barrier();
    }
    // TODO: yield to the scheduler
  }
}

void mutex_lock::unlock(void) {
  while(true) {
    if (compare_and_swap(&locked, 1, 0) == 0) {
      // TODO: check
      memory_barrier();
    }
    // TODO: yield to the scheduler
  }
}

bool mutex_lock::is_locked(void) { return locked; }
