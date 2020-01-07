#include <cpu.h>
#include <lock.h>
#include <printk.h>
#include <sched.h>

// #define LOCK_DEBUG

#ifdef LOCK_DEBUG
#define INFO(fmt, args...) printk("[MUTEX] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

static inline u64 xchg(volatile int* addr, int newval) {
  int result;
  // The + in "+m" denotes a read-modify-write operand.
  asm volatile("lock; xchgl %0, %1"
               : "+m"(*addr), "=a"(result)
               : "1"(newval)
               : "cc");
  return result;
}

static inline u32 CAS(volatile u32* mem, u32 newval, u32 oldval) {
  u32 ret;
  asm volatile("cmpxchgl %2, %1"
               : "=a"(ret), "+m"(*mem)
               : "r"(newval), "0"(oldval)
               : "cc", "memory");
  return ret;
}

#define memory_barrier() asm volatile("" ::: "memory")

void spinlock::lock(void) { mutex::lock(locked); }

void spinlock::unlock(void) { mutex::unlock(locked); }

bool spinlock::is_locked(void) { return locked; }

void mutex::lock(int& l) {
  while (!__sync_bool_compare_and_swap(&l, 0, 1)) {
    asm("pause");
  }
}

void mutex::unlock(int& l) { l = 0; }

int rwlock::rlock(void) {
  m_lock.lock();
  m_readers++;
  m_lock.unlock();
  return 0;
}
int rwlock::runlock(void) {
  m_lock.lock();
  m_readers--;
  m_lock.unlock();
  return 0;
}

int rwlock::wlock(void) {
  while (1) {
    m_lock.lock();

    if (likely(m_readers == 0)) {
      break;
    } else {
      m_lock.unlock();
      /* TODO: we should yield if we're not spread across cores */
    }
  }
  return 0;
}
int rwlock::wunlock(void) {
  m_lock.unlock();
  return 0;
}
