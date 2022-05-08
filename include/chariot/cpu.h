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
#include <realtime.h>

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




extern int processor_count;



struct sleep_waiter;
struct thread_context;


namespace cpu {

  /* This state is a local state to each processor */
  struct Core {
    // The processor ID
    // on x86, this comes from the apic
    // on RISC-V, this comes from mhartid after boot
    int id;
    void *local;

    // set if the core woke up a thread in this irq context
    bool woke_someone_up = false;
    bool primary = false;     // this core was the bootstrap processor
    bool active = false;      // filled out by the arch's initialization routine
    bool in_sched = false;    // the CPU has reached the scheduler
    bool timekeeper = false;  // this CPU does timekeeping stuff.

    rt::Scheduler local_scheduler;

    // the intrusive linked list into the list of all cores
    // Typically not locked, as they are only populated at boot.
    struct list_head cores;

    struct kstat_cpu kstat;

    unsigned long ticks_per_second = 0;

    spinlock sleepers_lock;
    struct sleep_waiter *sleepers = NULL;
    struct thread_context *sched_ctx;

    ck::ref<Thread> current_thread;
    ck::ref<Thread> next_thread;

    // locked by the source core, unlocked by the target core
    spinlock xcall_lock;
    struct xcall_command xcall_command;


#ifdef CONFIG_X86
    // The APIC and the IOApic for this core
    x86::Apic apic;
    x86::IOApic ioapic;
#endif

		Core(void);

    void prep_xcall(xcall_t func, void *arg, int *count) {
      xcall_lock.lock();
      xcall_command = {func, arg, count};
    }
  };

  extern struct list_head cores;
  cpu::Core *get(void);

  Process *proc(void);

  Thread *thread(void);
  bool in_thread(void);

  /**
   * These functions are all implemented in the arch directory
   */
  cpu::Core &current(void);
  // setup CPU segment descriptors, run once per cpu
  void seginit(cpu::Core *c, void *local = nullptr);
  void switch_vm(ck::ref<Thread>);
  inline u64 get_ticks(void) { return current().kstat.ticks; }


  int nproc(void);

  void add(cpu::Core *cpu);
  cpu::Core *get(int core);

  template <typename T>
  void each(T cb) {
    cpu::Core *core;
    list_for_each_entry(core, &cpu::cores, cores) { cb(core); }
  }

  void xcall(int core, xcall_t func, void *arg);
  inline void xcall_all(xcall_t func, void *arg) { return cpu::xcall(-1, func, arg); }

  void run_pending_xcalls(void);
}  // namespace cpu



// Nice macros to allow cleaner access to the current task and proc
#define curthd cpu::thread()
#define curproc cpu::proc()

static inline auto &core(void) { return cpu::current(); }
static inline auto &core_id(void) { return core().id; }
