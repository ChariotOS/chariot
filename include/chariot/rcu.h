#pragma once
#include <asm.h>  // barrier

// This version of the RCU is based heavily on the "toy" implementation from
// https://www.kernel.org/doc/Documentation/RCU/whatisRCU.txt. It's not
// perfect, but apparently it works -- just slowly.

#define smp_store_release(p, v) \
  do {                          \
    barrier();                  \
    WRITE_ONCE(*p, v);          \
  } while (0)


#define rcu_assign_pointer(p, v) ({ __atomic_store_n(&(p), (v), __ATOMIC_RELEASE); })

#define rcu_dereference(p)                \
  ({                                      \
    typeof(p) _________p1 = READ_ONCE(p); \
    (_________p1);                        \
  })

// kernel/rcu.cpp
extern void rcu_read_lock();
extern void rcu_read_unlock();

// STUBS
#define rcu_check_sparse(p, space)


/*
#define rcu_assign_pointer(p, v)                                                          \
  do {                                                                                    \
    uintptr_t _r_a_p__v = (uintptr_t)(v);                                                 \
    rcu_check_sparse(p, __rcu);                                                           \
                                                                                          \
    if (__builtin_constant_p(v) && (_r_a_p__v) == (uintptr_t)NULL)                        \
      WRITE_ONCE((p), (__decltype(p))(_r_a_p__v));                                        \
    else                                                                                  \
      __atomic_store((unsigned long *)&p, (unsigned long *)&_r_a_p__v, __ATOMIC_RELEASE); \
  } while (0)
        */
