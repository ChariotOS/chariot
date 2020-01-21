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

static inline int arch_atomic_swap(volatile int* x, int v) {
  asm("xchg %0, %1" : "=r"(v), "=m"(*x) : "0"(v) : "memory");
  return v;
}

static inline void arch_atomic_store(volatile int* p, int x) {
  asm("movl %1, %0" : "=m"(*p) : "r"(x) : "memory");
}

void spinlock::lock(void) { mutex::lock(locked); }

void spinlock::unlock(void) { mutex::unlock(locked); }

bool spinlock::is_locked(void) { return locked; }

static void spin_wait(volatile int* lock) { asm("pause"); }
void mutex::lock(volatile int& l) {
  volatile int* lock = &l;
  while (likely(arch_atomic_swap(lock, 1))) {
    spin_wait(lock);
  }
}

void mutex::unlock(volatile int& l) {
  volatile int* lock = &l;
  if (likely(lock[0])) {
    arch_atomic_store(lock, 0);
  }
}

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
