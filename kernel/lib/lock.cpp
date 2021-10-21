#include <cpu.h>
#include <lock.h>
#include <printk.h>
#include <sched.h>

#include <thread.h>

// #define LOCK_DEBUG

#ifdef LOCK_DEBUG
#define INFO(fmt, args...) printk("[MUTEX] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

#define ATOMIC_SET(thing) __atomic_test_and_set((thing), __ATOMIC_ACQUIRE)
#define ATOMIC_CLEAR(thing) __atomic_clear((thing), __ATOMIC_RELEASE)
#define ATOMIC_LOAD(thing) __atomic_load_n((thing), __ATOMIC_RELAXED)


static inline void arch_atomic_store(volatile int* p, int x) { __atomic_store((int*)p, &x, __ATOMIC_SEQ_CST); }

void spinlock::lock(void) {
  while (ATOMIC_SET(&locked)) {  // MESI protocol optimization
    while (__atomic_load_n(&locked, __ATOMIC_RELAXED) == 1) {
    }
  }
}

void spinlock::unlock(void) { ATOMIC_CLEAR(&locked); }


bool spinlock::is_locked(void) { return locked; }

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


bool spinlock::try_lock(void) {
  if (ATOMIC_SET(&locked)) return false;
  return true;
}

bool spinlock::lock_irqsave() {
  bool en = 0;
  while (1) {
    en = irq_disable_save();
    if (ATOMIC_SET(&locked) == 0) break;
    irq_enable_restore(en);

    // MESI protocol optimization
    while (__atomic_load_n(&locked, __ATOMIC_RELAXED) == 1) {
    }
  }
  return en;
}

bool spinlock::try_lock_irqsave(bool& success) {
  bool en = 0;
  while (1) {
    en = irq_disable_save();
    if (ATOMIC_SET(&locked) == 0) break;
    irq_enable_restore(en);
    success = false;
    return false;
  }
  success = true;
  return en;
}

void spinlock::unlock_irqrestore(bool flags) {
  held_by = 0;
  unlock();
  irq_enable_restore(flags);
}


static void spin_wait(volatile int* lock) { arch_relax(); }
void spinlock::lock(volatile int& l) {
  volatile int* lock = &l;
  while (likely(__sync_lock_test_and_set(lock, 1))) {
    spin_wait(lock);
  }
}



void spinlock::unlock(volatile int& l) { ATOMIC_CLEAR(&l); }

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
