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


static inline void arch_atomic_store(volatile int* p, int x) {
  __atomic_store((int*)p, &x, __ATOMIC_SEQ_CST);
}

void spinlock::lock(void) {
  while (__sync_lock_test_and_set(&locked, 1)) {
    /* nothing */
  }
}

void spinlock::unlock(void) {
  if (likely(locked)) {
    locked = 0;
  }
}


bool spinlock::is_locked(void) {
  return locked;
}

static inline bool irq_disable_save(void) {
  // preempt_disable();

  bool enabled = arch_irqs_enabled();

  if (enabled) {
    arch_disable_ints();
  }

  return enabled;
}

static inline void irq_enable_restore(bool enabled) {
  if (enabled) {
    /* Interrupts were originally enabled, so turn them back on */
    arch_enable_ints();
  }
}


bool spinlock::lock_irqsave() {
  bool en = 0;
  while (1) {
    en = irq_disable_save();
    if (__sync_lock_test_and_set(&locked, 1) == 0) break;
    if (cpu::current().in_sched) {
      // irq_enable_restore(en);
    }
    arch_relax();
  }
  if (cpu::in_thread())
    held_by = curthd->tid;
  else
    held_by = -2;
  return en;
}

void spinlock::unlock_irqrestore(bool flags) {
  held_by = 0;
  unlock();
  irq_enable_restore(flags);
}


static void spin_wait(volatile int* lock) {
  arch_relax();
}
void spinlock::lock(volatile int& l) {
  volatile int* lock = &l;
  while (likely(__sync_lock_test_and_set(lock, 1))) {
    spin_wait(lock);
  }
}


bool spinlock::try_lock(void) {
  if (__sync_lock_test_and_set(&locked, 1)) return false;
  return true;
}

void spinlock::unlock(volatile int& l) {
  volatile int* lock = &l;
  if (likely(lock[0])) {
    arch_atomic_store(lock, 0);
  }
}

int rwlock::read_lock(void) {
  m_lock.lock();
  m_readers++;
  m_lock.unlock();
  return 0;
}
int rwlock::read_unlock(void) {
  m_lock.lock();
  m_readers--;
  m_lock.unlock();
  return 0;
}

int rwlock::write_lock(void) {
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
int rwlock::write_unlock(void) {
  m_lock.unlock();
  return 0;
}
