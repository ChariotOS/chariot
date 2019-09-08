#include <lock.h>
#include <printk.h>

#define memory_barrier() asm volatile("" ::: "memory")

void mutex_lock::lock(void) {
  // printk("[MUTEX LOCK ] %p\n", this);
  while (__sync_lock_test_and_set(&locked, 1)) {
    /* nothing */
  }
}

void mutex_lock::unlock(void) {
  // printk("[MUTEX UNLCK] %p\n", this);
  __sync_lock_release(&locked);
}

bool mutex_lock::is_locked(void) { return locked; }
