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

// helper function to check if interrupts are enabled or not
static bool ints_enabled(void) { return readeflags() & 0x200; }

void mutex_lock::lock(void) { mutex::lock(locked); }

void mutex_lock::unlock(void) { mutex::unlock(locked); }

bool mutex_lock::is_locked(void) { return locked; }

void mutex::lock(int& l) {
  while (xchg(&l, 1) != 0) {
    if (!ints_enabled() && cpu::in_thread()) {
      sched::yield();
    }
  }
}

void mutex::unlock(int& l) { xchg(&l, 0); }
