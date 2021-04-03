// A single header file for all things task and scheduler related

#pragma once


#ifdef __cplusplus

#include "ptr.h"

#include <func.h>
#include <map.h>
#include <mm.h>
#include <sem.h>
#include <signals.h>
#include <string.h>
#include <vec.h>


#ifdef CONFIG_RISCV
#include <riscv/arch.h>
#endif


#define SIGBIT(n) (1llu << (n))

#define RING_KERN 0
#define RING_USER 3


#define PS_EMBRYO (-1)
/* The task is either actively running or is able to be run */
#define PS_RUNNING (0)
#define PS_INTERRUPTIBLE (1)
#define PS_UNINTERRUPTIBLE (2)
#define PS_ZOMBIE (3)


// #include <schedulers.h>



struct sigaction {
  void (*sa_handler)(int);
  void (*sa_sigaction)(int, long *sigset, void *);
  long sa_mask;
  int sa_flags;
  void (*sa_restorer)(void);
};


namespace sched {
  void block();
  void unblock(thread &, bool interrupt = false);

  // using impl = sched::round_robin;
}  // namespace sched

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

using pid_t = int;
using uid_t = int;
using gid_t = int;


struct process_user_info {
  // user and group information
  long uid = 0;
  long euid = 0;
  long gid = 0;
  long egid = 0;
};

/**
 * a process is a group of information that is shared among many tasks
 */
struct process final : public refcounted<struct process> {
  using ptr = ref<process>;

  pid_t pid;   // process id, and pid of the creator task
  pid_t pgid;  // process group id. Normally the same as the pid itself.
  pid_t ppid;  // process id of the parent process.

  /**
   * array of tasks that are in this process. The order doesn't matter and is
   * probably random.
   */
  vec<pid_t> threads;

  /**
   * when a process spawns a new process, it is an embryo. A process can only
   * pctl() a pid that is in this list. When a task startpid()'s a pid, it will
   * be moved from this list to the children list.
   */
  vec<process::ptr> children;

  unsigned ring = RING_USER;

  // signal information
  struct {
    /* This is the address of the signal's "return" trampoline. Basically, the
     * signal is triggered and this is the retunrn address of that signal. If
     * this is not set (== 0), signals cannot be sent, as it would result in a
     * segmentation fault [bad! :^)]. So the user must install this before any
     * signals will be sent - likely in the _start function. This function
     * should run the systemcall "sigreturn" in order to end the signal's
     * lifetime */
    off_t ret = 0;
    struct sigaction handlers[64] = {{0}};
    // void *handlers[64] = {0};
    spinlock lock;
  } sig;

  /*
   * A process contains a ref counted pointer to the parent.
   * TODO: is this unsafe?
   */
  struct process *parent;

  /**
   * Information about the user associated with this process. Inherited from
   * parent on spawn.
   */
  struct process_user_info user;

  /**
   * Metadata like name, arguments and envionment variables are converted
   * to kernel structures and stored in the process information.
   */
  string name;
  vec<string> args;
  vec<string> env;

  bool exited = 0;
  uint8_t exit_code = 0;
  uint8_t exit_signal = 0;

  mm::space *mm;


  ref<fs::file> executable;

  struct {
    bool exists = false;
    off_t fileoff;
    size_t memsz;
    size_t fsize;
  } tls_info;

  u64 create_tick = 0;
  // The current working directory of the process.
  fs::inode *cwd = nullptr;
  fs::inode *root = nullptr;
  string cwd_string;
  spinlock datalock;

  /* threads stuck in a waitpid() call */
  // semaphore waiters = semaphore(0);

  wait_queue child_wq;

  spinlock file_lock;
  map<int, ref<fs::file>> open_files;


  spinlock futex_lock;
  wait_queue &futex_queue(int *);
  map<off_t, unique_ptr<wait_queue>> m_futex_queues;

  /**
   * exec() - execute a command (implementation for startpid())
   */
  int exec(string &path, vec<string> &argv, vec<string> &envp);

  ref<fs::file> get_fd(int fd);
  int add_fd(ref<fs::file>);

  pid_t create_thread(void *ip, int state);

  // just walks the threads and checks they are all zombie
  bool is_dead(void);

  void terminate(int signal);

  inline process() {
  }
  process(const process &) = delete;
  ~process(void);
};

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

	thread *next;
	thread *prev;
};

struct thread_locks {
  spinlock generic;  // locked for data manipulation
  spinlock run;      // locked while a thread is being run
};

struct thread_waitqueue_info {
  unsigned waiting_on = 0;
  int flags = 0;
  bool rudely_awoken = false;
  // intrusive list for the waitqueue. It's the first if wq_prev == NULL
};


struct thread final {
  pid_t tid;  // unique thread id
  pid_t pid;  // process id. If this thread is the main thread, tid == pid

  // A reference to the process this thread belongs to.
  process &proc;

  volatile int state;
  int exit_code;

  int kerrno = 0;
  bool preemptable = true;

  struct kernel_stack {
    void *start;
    long size;
  };

  off_t yield_from = 0;

  // If this lock is not null, it is released after the runlock is released.
  // If this is not null, it is acquired after the runlock is acquired
  spinlock *held_lock = nullptr;

  /* Reference to the kernel stack */
  vec<kernel_stack> stacks;
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
  struct thread_locks locks;

  // register contexts
  struct thread_context *kern_context;
  reg_t *trap_frame;
  off_t userspace_sp = 0;
  struct thread_waitqueue_info wq;

  // Threads who are joining on this thread.
  wait_queue joiners;
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
  off_t setup_tls(void);
  static thread *lookup(pid_t);
  static thread *lookup_r(pid_t);
  static bool teardown(thread *);
  // sends a signal to the thread and returns if it succeeded or not
  bool send_signal(int sig);

  /**
   * Do not use this API, go through sched::proc::* to allocate and deallocate
   * processes and threads.
   */
  thread(pid_t tid, struct process &);
  thread(const thread &) = delete;  // no copy
  ~thread(void);

  // return a list of instruction pointers (recent -> older)
  vec<off_t> backtrace(off_t rbp, off_t rip);
};


namespace sched {


  // run a signal on the current thread
  void dispatch_signal(int sig);

  /*
   * Claims a signal to be handled next. This is called from the
   * architecture before returning to userspace.
   *
   * Returns -1 if there is no signal, 0 if there is one
   *
   * This function may not return if the default action (or the required action)
   * is to kill the process.
   */
  int claim_next_signal(int &sig, void *&handler);

  bool init(void);

  bool enabled();

#define SCHED_MLFQ_DEPTH 10
#define PRIORITY_HIGH (SCHED_MLFQ_DEPTH - 1)
#define PRIORITY_IDLE 0

  process &kernel_proc(void);

  void set_state(int state);



  enum yieldres { None, Interrupt };

  yieldres yield(spinlock *held_lock = nullptr);
  yieldres do_yield(int status);



  // does not return
  void run(void);

  void dumb_sleepticks(unsigned long);

  void handle_tick(u64 tick);

  // force the process to exit, (yield with different state)
  void exit();

  int remove_task(struct thread *t);
  int add_task(struct thread *);

  // called before dropping back into user space. This is needed
  // when a thread should not return to userspace because it must
  // die or something else
  void before_iret(bool userspace);

  namespace proc {
    struct spawn_options {
      int ring = RING_USER;
    };

#define SPAWN_KERN (1 << 0)

#define SPAWN_FORK (1 << 1)
    // #define SPAWN_VFORK (1 << 1)
    process::ptr spawn_process(struct process *parent, int flags);

    // get the kernel process (creating if it doesnt exist
    struct process *kproc(void);

    pid_t create_kthread(const char *name, int (*func)(void *), void *arg = NULL);

    struct thread *spawn_kthread(const char *name, int (*func)(void *), void *arg = NULL);

    void dump_table();

    // spawn init (pid 1) and try to execute each of the paths in order,
    // stopping on the first success. Returns -1
    pid_t spawn_init(vec<string> &paths);

    /* remove a process from the ptable */
    bool ptable_remove(pid_t);

    int send_signal(pid_t, int sig);

    int reap(process::ptr);

    int do_waitpid(pid_t, int &status, int options);

    // this takes a lock over the process table while iterating
    // Return false from the callback to stop.
    void in_pgrp(pid_t pgid, func<bool(struct process &)>);


  };  // namespace proc

}  // namespace sched

#endif
