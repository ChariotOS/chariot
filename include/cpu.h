#pragma once

#include <lock.h>
#include <types.h>
#include <sched.h>

struct cpu_t {
  void *local;
  int ncli = 0;
  size_t ticks = 0;
  size_t intena = 0;

  int current_trap = -1;
  int trap_depth = 0;

  uint16_t preemption_depth = 0;

  u32 speed_khz;
  struct thread *current_thread;
  struct thread_context *sched_ctx;
};

// Nice macros to allow cleaner access to the current task and proc
#define curthd cpu::thread()
#define curproc cpu::proc()


namespace cpu {

// return the current cpu struct
cpu_t &current(void);
cpu_t *get(void);

struct process *proc(void);

struct thread *thread(void);
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

void switch_vm(struct thread *);

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
