#pragma once

#include <lock.h>
#include <types.h>
#include <task.h>

struct cpu_t {
  void *local;
  int ncli = 0;
  size_t ticks = 0;
  size_t intena = 0;

  int current_trap = -1;
  int trap_depth = 0;

  uint16_t preemption_depth = 0;

  u32 speed_khz;
  ref<struct task> current_thread;
  struct task_context *sched_ctx;
};

namespace cpu {

// return the current cpu struct
cpu_t &current(void);
cpu_t *get(void);

ref<struct task_process> proc(void);

ref<struct task> task(void);
bool in_thread(void);

// setup CPU segment descriptors, run once per cpu
void seginit(void *local = nullptr);

void calc_speed_khz(void);

// pushcli - disable interrupts and incrementing the depth if interrupt
// disable-ness (?)
void pushcli();
// popcli - pop the depth of disabled interrupts, and if it reaches zero, enable
// interrupts again
void popcli();

void switch_vm(struct task *);

inline int ncli(void) { return current().ncli; }

static inline u64 get_ticks(void) { return current().ticks; }

static inline void sleep_ms(int ms) {
  auto start = get_ticks();
  while (1) {
    if (get_ticks() > start + ms) break;
    // TODO: yield if there is enough time left?
  }
}


void preempt_enable(void);
void preempt_disable(void);
// re-enable
void preempt_reset(void);
// is it disabled?
int preempt_disabled(void);

}  // namespace cpu
