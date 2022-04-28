#pragma once


// this is all stolen from glibc :^)

#define atomic_thread_fence_acquire() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define atomic_thread_fence_release() __atomic_thread_fence(__ATOMIC_RELEASE)
#define atomic_thread_fence_seq_cst() __atomic_thread_fence(__ATOMIC_SEQ_CST)

#define atomic_load_relaxed(mem) ({ __atomic_load_n((mem), __ATOMIC_RELAXED); })
#define atomic_load_acquire(mem) ({ __atomic_load_n((mem), __ATOMIC_ACQUIRE); })

#define atomic_store_relaxed(mem, val)                \
  do {                                                \
    __atomic_store_n((mem), (val), __ATOMIC_RELAXED); \
  } while (0)
#define atomic_store_release(mem, val)                \
  do {                                                \
    __atomic_store_n((mem), (val), __ATOMIC_RELEASE); \
  } while (0)

/* On failure, this CAS has memory_order_relaxed semantics.  */
#define atomic_compare_exchange_weak_relaxed(mem, expected, desired) \
  ({ __atomic_compare_exchange_n((mem), (expected), (desired), 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED); })
#define atomic_compare_exchange_weak_acquire(mem, expected, desired) \
  ({ __atomic_compare_exchange_n((mem), (expected), (desired), 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED); })
#define atomic_compare_exchange_weak_release(mem, expected, desired) \
  ({ __atomic_compare_exchange_n((mem), (expected), (desired), 1, __ATOMIC_RELEASE, __ATOMIC_RELAXED); })

#define atomic_exchange_relaxed(mem, desired) ({ __atomic_exchange_n((mem), (desired), __ATOMIC_RELAXED); })
#define atomic_exchange_acquire(mem, desired) ({ __atomic_exchange_n((mem), (desired), __ATOMIC_ACQUIRE); })
#define atomic_exchange_release(mem, desired) ({ __atomic_exchange_n((mem), (desired), __ATOMIC_RELEASE); })

#define atomic_fetch_add_relaxed(mem, operand) ({ __atomic_fetch_add((mem), (operand), __ATOMIC_RELAXED); })
#define atomic_fetch_add_acquire(mem, operand) ({ __atomic_fetch_add((mem), (operand), __ATOMIC_ACQUIRE); })
#define atomic_fetch_add_release(mem, operand) ({ __atomic_fetch_add((mem), (operand), __ATOMIC_RELEASE); })
#define atomic_fetch_add_acq_rel(mem, operand) ({ __atomic_fetch_add((mem), (operand), __ATOMIC_ACQ_REL); })

#define atomic_fetch_and_relaxed(mem, operand) ({ __atomic_fetch_and((mem), (operand), __ATOMIC_RELAXED); })
#define atomic_fetch_and_acquire(mem, operand) ({ __atomic_fetch_and((mem), (operand), __ATOMIC_ACQUIRE); })
#define atomic_fetch_and_release(mem, operand) ({ __atomic_fetch_and((mem), (operand), __ATOMIC_RELEASE); })

#define atomic_fetch_or_relaxed(mem, operand) ({ __atomic_fetch_or((mem), (operand), __ATOMIC_RELAXED); })
#define atomic_fetch_or_acquire(mem, operand) ({ __atomic_fetch_or((mem), (operand), __ATOMIC_ACQUIRE); })
#define atomic_fetch_or_release(mem, operand) ({ __atomic_fetch_or((mem), (operand), __ATOMIC_RELEASE); })

#define atomic_fetch_xor_release(mem, operand) ({ __atomic_fetch_xor((mem), (operand), __ATOMIC_RELEASE); })
