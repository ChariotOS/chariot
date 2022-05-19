#pragma once

#include <ck/ptr.h>
#include <ck/vec.h>
#include <ck/string.h>

#include <lock.h>
#include <arch.h>
#include <wait.h>

#include <fwd.h>
#include <realtime.h>

#ifdef CONFIG_RISCV
#include <riscv/arch.h>
#endif

#ifdef CONFIG_X86
struct ThreadContext {
  unsigned long r15;
  unsigned long r14;
  unsigned long r13;
  unsigned long r12;
  unsigned long r11;
  unsigned long rbx;
  unsigned long rbp;  // rbp
  unsigned long pc;   // rip;
};
#endif


#ifdef CONFIG_RISCV
struct ThreadContext {
  rv::xsize_t pc;  // 0
  // callee-saved
  rv::xsize_t s0;   // 8
  rv::xsize_t s1;   // 16
  rv::xsize_t s2;   // 24
  rv::xsize_t s3;   // 32
  rv::xsize_t s4;   // 40
  rv::xsize_t s5;   // 48
  rv::xsize_t s6;   // 56
  rv::xsize_t s7;   // 64
  rv::xsize_t s8;   // 72
  rv::xsize_t s9;   // 80
  rv::xsize_t s10;  // 88
  rv::xsize_t s11;  // 96
  rv::xsize_t ra;   // return address
};
#endif


#ifdef CONFIG_ARM64
struct ThreadContext {
  // svc mode registers
  unsigned long r4;
  unsigned long r5;
  unsigned long r6;
  unsigned long r7;
  unsigned long r8;
  unsigned long r9;
  unsigned long r10;
  unsigned long r11;
  unsigned long r12;
  unsigned long r13;
  unsigned long r14;
  unsigned long r15;
  unsigned long r16;
  unsigned long r17;
  unsigned long r18;
  unsigned long r19;
  unsigned long r20;
  unsigned long r21;
  unsigned long r22;
  unsigned long r23;
  unsigned long r24;
  unsigned long r25;
  unsigned long r26;
  unsigned long r27;
  unsigned long r28;
  unsigned long r29;
  unsigned long pc;  // technically, this register is lr (x30), but chariot expects "pc"
};
#endif




struct ThreadFPUState {
  bool initialized = false;
  void *state;
};


struct ThreadStats {
  u64 syscall_count = 0;
  u64 run_count = 0;

  int current_cpu = -1;
  int last_cpu = -1;

  u64 cycles = 0;
  u64 last_start_cycle = 0;
};


struct KernelStack {
  void *start;
  long size;
};

struct SignalConfig {
  unsigned long pending = 0;
  unsigned long mask = 0;
  long handling = -1;
  void *arch_priv = nullptr;
};


struct Thread final : public ck::weakable<Thread> {
 public:
  friend rt::Scheduler;
  friend rt::PriorityQueue;
  friend rt::Queue;

  long tid;                      // The thread ID of this thread
  long pid;                      // The process ID of the process this thread belongs to
  Process &proc;                 // A reference to the process this thread belongs to
  volatile int state;            // The thread state (RUNNING, INTERRUPTABLE, etc..)
  int exit_code;                 // The value passed to exit_thread()
  int kerrno = 0;                // A per-thread errno value
  bool preemptable = true;       // If the thread can be preempted
  ThreadFPUState fpu;            // Floating point state
  ThreadStats stats;             // Thread statistics
  spinlock runlock;              // Held while the thread is running. This is a dumb sanity check
  ThreadContext *kern_context;   // The register state to switch to
  reg_t *trap_frame;             // The current trap frame
  bool rudely_awoken = false;    // if this thread was woken rudely for a signal or something
  ck::vec<KernelStack> stacks;   // A stack of kernel stacks
  SignalConfig sig;              // Thread signal configuration
  wait_queue joiners;            // Threads who are joining on this thread
  spinlock joinlock;             // Held when someone is joining (tearing this thread down).
  long ktime_us = 0;             // Time attributed to kernelspace
  long utime_us = 0;             // Time attributed to userspace
  long last_start_utime_us = 0;  // the last time that this thread
  off_t tls_uaddr = 0;           // the location of the thread local storage for this thread
  size_t tls_usize = 0;          // how big the thread local storage is
  ck::string name;               // The name of this thread
  bool should_die = false;       // the thread needs to be torn down. Must not return to userspace
  bool kern_idle = false;        // the thread is a kernel idle thread

  uint64_t timeslice = 1;
  uint64_t ticks_ran = 0;  // how many ticks this thread has run for

  uint64_t start_time = 0;    // when the task got last started
  uint64_t cur_run_time = 0;  // how long it has run so far without being preempted
  uint64_t run_time = 0;      // How long this thread has ran (not sure how its different)
  uint64_t deadline = 0;      // current deadline / time of next arrival if pending
  uint64_t exit_time = 0;     // Time of competion after being run


  // Statistics that are reset when the constraints are changed
  uint64_t arrival_count = 0;           // how many times it has arrived (1 for aperiodic/sporadic)
  uint64_t resched_count = 0;           // how many times resched was invoked on this thread
  uint64_t resched_long_count = 0;      // how many times the long path was taken for the thread
  uint64_t switch_in_count = 0;         // number of times switched to
  uint64_t miss_count = 0;              // number of deadline misses
  uint64_t miss_time_sum = 0;           // sum of missed time
  uint64_t miss_time_sum2 = 0;          // sum of squares of missed time
  rt::TaskStatus rt_status;             // Status of this task in the realtime system (Different from state)
  struct rb_node prio_node;             // Intrusive placement int oa `rt::PriorityQueue`
  struct list_head queue_node;          // intrusive placement structure into an `rt::Queue`
  rt::TaskQueue *current_queue = NULL;  // Track if this task is queued somewhere, and if it is, which one?
  rt::Constraints m_constraint;         // The realtime constraints of this task
  rt::Scheduler *scheduler = NULL;      // What scheduler currently controls this Task
  spinlock queuelock;                   // held while moving a thread to a different queue (wait or scheduler)

  void set_state(int st);                       // change the thread state (this->state)
  int get_state(void);                          // get the thread state (this->state) in a "safe" way
  void setup_stack(reg_t *);                    // Setup the the stack given some register state
  void interrupt(void);                         // Notify a thread that a signal is avail, interrupting it from a waitqueue if avail
  bool kickoff(void *rip, int state);           // Tell a thread to start running at some RIP
  static ck::ref<Thread> lookup(long);          // Lookup thread by TID
  static ck::ref<Thread> lookup_r(long);        // ^ (but unlocked)
  static bool teardown(ck::ref<Thread> &&thd);  // Teardown this thread
  static bool join(ck::ref<Thread> thd);        // Join on some thread. (Wait for it to exit)
  static void dump(void);                       // Dump the thread table state

  // Add this thread to a scheduler queue. Optionally admitting it. If the scheduler
  // cannot admit or cannot add the task for any reason, this function will return
  // a non-zero value.
  int make_runnable(int cpu = RT_CORE_SELF, bool admit = true);

  // sends a signal to the thread and returns if it succeeded or not
  bool send_signal(int sig);

  // tell the thread to exit. Wait on joiners to know when it finished exiting
  void exit(void);
  // context switch into this thread until it yields
  void run(void);
  // return a list of instruction pointers (recent -> older)
  ck::vec<off_t> backtrace(off_t rbp, off_t rip);

  // get access to the constraint for this Thread
  auto &constraint(void) { return m_constraint; }
  void set_constraint(rt::Constraints &c) { m_constraint = c; }
  rt::Scheduler *current_scheduler(void) const { return scheduler; }
  void remove_from_scheduler();
  void reset_state();
  void reset_stats();

  // Do not use this API, go through sched::proc::* to allocate and deallocate
  // processes and threads.
  Thread(long tid, Process &);
  Thread(const Thread &) = delete;  // no copy
  ~Thread(void);



 protected:
};
