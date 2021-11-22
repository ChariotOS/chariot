#pragma once

#include <lock.h>
// #include <sched.h>
// #include <sleep.h>
#include <types.h>
#include <ck/ptr.h>
#include <ck/vec.h>
#include <cpu_usage.h>
#include <fwd.h>
#include <list_head.h>

#ifdef CONFIG_X86
#include <x86/apic.h>
#include <x86/ioapic.h>
#endif


struct kstat_cpu {
  unsigned long ticks;         // total ticks
  unsigned long user_ticks;    // user tasks
  unsigned long kernel_ticks;  // kernel tasks
  unsigned long idle_ticks;    // idle

  unsigned long last_tick_tsc;
  unsigned long tsc_per_tick;
};


typedef void (*xcall_t)(void *);

struct xcall_command {
	// the function to be called on the cpu
	xcall_t fn;
	// the single argument to the function when called.
	void *arg;
	// decremented when an xcall is completed
	int *count = nullptr;
};


/* This state is a local state to each processor */
struct processor_state {
  void *local;
  int id;

  bool primary = false;
	bool active = false; // filled out by the arch's initialization routine
  bool in_sched = false;

  struct kstat_cpu kstat;

  // this cpu is the timekeeper
  bool timekeeper = false;
  unsigned long ticks_per_second = 0;

  spinlock sleepers_lock;
  struct sleep_waiter *sleepers = NULL;


  u32 speed_khz;
  ck::ref<Thread> current_thread;
  // filled in by "pick next thread" in the scheduler
  ck::ref<Thread> next_thread;

  struct thread_context *sched_ctx;

	spinlock xcall_lock;
	ck::vec<struct xcall_command> xcall_commands;


#ifdef CONFIG_X86
	// The APIC and the IOApic for this core
	x86::Apic apic;
	x86::IOApic ioapic;

	// per cpu apic location
	uint32_t *lapic = NULL;
#endif

  /* The depth of interrupts. If this is not zero, we aren't in an interrupt context */
  int interrupt_depth = 0;

  // set if the core woke up a thread in this irq context
  bool woke_someone_up = false;


	struct list_head cores;
	void prep_xcall(xcall_t func, void *arg, int *count) {
		scoped_irqlock l(xcall_lock);
		xcall_commands.push({func, arg, count});
	}
};


extern int processor_count;
// extern struct processor_state cpus[CONFIG_MAX_CPUS];



namespace cpu {

	extern struct list_head cores;
  struct processor_state *get(void);

  Process *proc(void);

  Thread *thread(void);
  bool in_thread(void);

  /**
   * These functions are all implemented in the arch directory
   */
  struct processor_state &current(void);
  // setup CPU segment descriptors, run once per cpu
  void seginit(struct processor_state *c, void *local = nullptr);
  void switch_vm(ck::ref<Thread>);
  inline u64 get_ticks(void) { return current().kstat.ticks; }


  int nproc(void);

	void add(struct processor_state *cpu);
	struct processor_state *get(int core);

	template<typename T>
	void each(T cb) {
		struct processor_state *core;
		list_for_each_entry(core, &cpu::cores, cores) {
			cb(core);
		}
	}

  void xcall(int core, xcall_t func, void *arg, bool wait = true);
  inline void xcall_all(xcall_t func, void *arg, bool wait = true) { return cpu::xcall(-1, func, arg, wait); }

	void run_pending_xcalls(void);
}  // namespace cpu



// Nice macros to allow cleaner access to the current task and proc
#define curthd cpu::thread()
#define curproc cpu::proc()

static inline auto &core(void) { return cpu::current(); }
static inline auto &core_id(void) { return core().id; }
