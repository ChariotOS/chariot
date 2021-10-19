#pragma once

#include <ck/ptr.h>
#include <ck/vec.h>

#include <lock.h>
#include <arch.h>
#include <wait.h>


#ifdef CONFIG_RISCV
#include <riscv/arch.h>
#endif

#ifdef CONFIG_X86
struct thread_context {
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
struct thread_context {
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
struct thread_context {
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
  unsigned long pc;  // technically, this register is lr (x30), but chariot expects
};
#endif




struct thread_fpu_info {
  bool initialized = false;
  void *state;
};


struct thread_statistics {
  u64 syscall_count = 0;
  u64 run_count = 0;

  int current_cpu = -1;
  int last_cpu = -1;

  u64 cycles = 0;
  u64 last_start_cycle = 0;
};

struct thread_sched_info {
  int has_run = 0;
  int timeslice = 1;
  int priority = 0;  // highest priority to start out. Only gets worse :)

  int good_streak = 0;
};


struct thread_waitqueue_info {
  unsigned waiting_on = 0;
  int flags = 0;
  bool rudely_awoken = false;
  // intrusive list for the waitqueue. It's the first if wq_prev == NULL
};



// forward declaration
class process;

struct kernel_stack {
  void *start;
  long size;
};


struct thread final : public ck::weakable<struct thread> {
 public:
  long tid;
  long pid;
  // put these at the top for "cache" reasons idk
  ck::ref<thread> next;
  ck::ref<thread> prev;

  process &proc;

  volatile int state;
  int exit_code;

  int kerrno = 0;
  bool preemptable = true;



  off_t yield_from = 0;

  // If this lock is not null, it is released after the runlock is released.
  // If this is not null, it is acquired after the runlock is acquired
  spinlock *held_lock = nullptr;

  /* Reference to the kernel stack */
  ck::vec<kernel_stack> stacks;

  // Masks are per-thread
  struct {
    unsigned long pending = 0;
    unsigned long mask = 0;
    long handling = -1;
    void *arch_priv = nullptr;
  } sig;

  // sched::impl::thread_state sched_state;
  struct thread_sched_info sched;
  struct thread_fpu_info fpu;
  struct thread_statistics stats;
  // bundle locks into a single struct
  spinlock runlock;



  // register contexts
  struct thread_context *kern_context;
  reg_t *trap_frame;
  off_t userspace_sp = 0;
  struct thread_waitqueue_info wq;

  // Threads who are joining on this thread.
  struct wait_queue joiners;


  /* This is simply a flag. Locked when someone is joining (tearing down) this thread.
   * Other threads attempt to lock it. If the lock is already held, fail "successfully"
   */
  spinlock joinlock;
  /* Time spent in kernelspace */
  long ktime_us = 0;
  /* Time spent in userspace */
  long utime_us = 0;
  /* The last time that the kernel entered userspace */
  long last_start_utime_us = 0;

  off_t tls_uaddr;
  size_t tls_usize;

  union /* flags */ {
    u64 flags = 0;
    struct /* bitmask */ {
      unsigned kthread : 1;
      unsigned should_die : 1;  // the thread needs to be torn down, must not
                                // return to userspace
      unsigned kern_idle : 1;   // the thread is a kernel idle thread
      unsigned exited : 1;      // the thread has exited, it's safe to delete when
                                // reaped by the parent or another thread
    };
  };


  void set_state(int st);
  int get_state(void);
  void setup_stack(reg_t *);
  // Notify a thread that a signal is available, interrupting it from a
  // waitqueue if there is one
  void interrupt(void);
  // tell the thread to start running at a certain address.
  bool kickoff(void *rip, int state);
  static ck::ref<thread> lookup(long);
  static ck::ref<thread> lookup_r(long);


  // remove the thread from all queues it is a member of
  static bool teardown(ck::ref<thread> &&thd);
  static bool join(ck::ref<thread> thd);

  // sends a signal to the thread and returns if it succeeded or not
  bool send_signal(int sig);



  static void dump(void);
  /**
   * Do not use this API, go through sched::proc::* to allocate and deallocate
   * processes and threads.
   */
  thread(long tid, process &);
  thread(const thread &) = delete;  // no copy
  ~thread(void);

  // return a list of instruction pointers (recent -> older)
  ck::vec<off_t> backtrace(off_t rbp, off_t rip);
};
