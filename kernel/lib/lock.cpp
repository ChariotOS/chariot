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

static inline void arch_atomic_store(volatile int* p, int x) { asm("movl %1, %0" : "=m"(*p) : "r"(x) : "memory"); }

void spinlock::lock(void) {
  while (1) {
    if (arch_atomic_swap(&locked, 1) == 0) break;
    if (cpu::current().in_sched) {
      // sched::yield();
    }
  }
}

void spinlock::unlock(void) {
  if (likely(locked)) {
    arch_atomic_store(&locked, 0);
  }
}


void spinlock::lock_cli(void) {
  while (1) {
    arch_disable_ints();
    if (arch_atomic_swap(&locked, 1) == 0) break;
    if (cpu::current().in_sched) {
      arch_enable_ints();
      // sched::yield();
    }
  }
}

void spinlock::unlock_cli(void) {
  if (likely(locked)) {
    arch_atomic_store(&locked, 0);
  }
  arch_enable_ints();
}

bool spinlock::is_locked(void) { return locked; }



// Do not directly use these functions or sti/cli unless you know
// what you are doing...
// Instead, use irq_disable_save and a matching irq_enable_restore

static inline uint64_t read_rflags(void) {
  uint64_t ret;
  asm volatile("pushfq; popq %0" : "=a"(ret));
  return ret;
}
static inline uint8_t irqs_enabled(void) {
  uint64_t rflags = read_rflags();
  return (rflags & RFLAGS_IF) != 0;
}

static inline uint8_t irq_disable_save(void) {
  // preempt_disable();

  uint8_t enabled = irqs_enabled();

  if (enabled) {
    arch_disable_ints();
  }

  return enabled;
}

static inline void irq_enable_restore(uint8_t iflag) {
  if (iflag) {
    /* Interrupts were originally enabled, so turn them back on */
    arch_enable_ints();
  }
}


unsigned long spinlock::lock_irqsave() {
  unsigned long flags = 0;
  while (1) {
    flags = irq_disable_save();
    if (arch_atomic_swap(&locked, 1) == 0) break;
    if (cpu::current().in_sched) {
      irq_enable_restore(flags);
    }
  }
  return flags;
}

void spinlock::unlock_irqrestore(unsigned long flags) {
  unlock();
  irq_enable_restore(flags);
}


static void spin_wait(volatile int* lock) { arch_relax(); }
void spinlock::lock(volatile int& l) {
  volatile int* lock = &l;
  while (likely(arch_atomic_swap(lock, 1))) {
    spin_wait(lock);
  }
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
