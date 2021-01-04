#pragma once

#include <lock.h>
#include <sched.h>
#include <sleep.h>
#include <types.h>

struct kstat_cpu {
  unsigned long ticks;   // total ticks
  unsigned long uticks;  // user tasks
  unsigned long kticks;  // kernel tasks
  unsigned long iticks;  // idle

  unsigned long last_tick_tsc = 0;
  unsigned long tsc_per_tick = 0;
};


/* This state is a local state to each processor */
struct processor_state {
  void *local;
  int cpunum;

  bool in_sched = false;

  struct kstat_cpu kstat;

  // this cpu is the timekeeper
  bool timekeeper = false;
  unsigned long ticks_per_second = 0;

  spinlock sleepers_lock;
  struct sleep_waiter *sleepers = NULL;

  u32 speed_khz;
  struct thread *current_thread;
  // filled in by "pick next thread" in the scheduler
  struct thread *next_thread;

  struct thread_context *sched_ctx;

	/* The depth of interrupts. If this is not zero, we aren't in an interrupt context */
	int interrupt_depth = 0;
};

extern int cpunum;
extern struct processor_state cpus[CONFIG_MAX_CPUS];

// Nice macros to allow cleaner access to the current task and proc
#define curthd cpu::thread()
#define curproc cpu::proc()

namespace cpu {

  struct processor_state *get(void);

  struct process *proc(void);

  struct thread *thread(void);
  bool in_thread(void);

  void calc_speed_khz(void);

  /**
   * These functions are all implemented in the arch directory
   */
  struct processor_state &current(void);
  // setup CPU segment descriptors, run once per cpu
  void seginit(void *local = nullptr);
  void switch_vm(struct thread *);
  inline u64 get_ticks(void) { return current().kstat.ticks; }


  int nproc(void);

}  // namespace cpu
