#include <rcu.h>
#include <cpu.h>
#include <lock.h>

// define the global rwlock
rwlock rcu_gp_mutex;

void rcu_read_lock(void) {
  core().preempt_count++;
  barrier();
  rcu_gp_mutex.read_lock();
}

void rcu_read_unlock() {
  core().preempt_count--;
  barrier();
  rcu_gp_mutex.read_unlock();
}
