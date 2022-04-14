#pragma once

#include <list_head.h>
#include <types.h>
#include <wait.h>

// fwd decl
namespace cpu { struct Core; } 

struct sleep_waiter {
  struct sleep_waiter *prev;
  struct sleep_waiter *next;
  cpu::Core *cpu;
  uint64_t wakeup_us = 0;
  wait_queue wq;

  sleep_waiter() = default;
  inline sleep_waiter(uint64_t us) {
    start(us);
  }

  /* Removes if needed */
  ~sleep_waiter();

  /* Add the blocker to the queue */
  void start(uint64_t us);

  /* Just wait on the wait queue */
  wait_result wait(void);

  /* Remove from the CPU structure */
  void remove(void);
};

/* returns 0 or -ERRNO */
int do_usleep(uint64_t us);
/* Check if any threads need to be awoken, and return true if there were any */
bool check_wakeups(void);
