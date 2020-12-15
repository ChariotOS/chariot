// A single header file for all things task and scheduler related

#pragma once

#include <ptr.h>

#include <func.h>
#include <map.h>
#include <mm.h>
#include <sem.h>
#include <signals.h>
#include <string.h>
#include <vec.h>
#define SIGBIT(n) (1llu << (n))

#define RING_KERN 0
#define RING_USER 3

/*
#define PS_UNRUNNABLE (-1)
#define PS_RUNNABLE (0)
#define PS_ZOMBIE (1)
#define PS_BLOCKED (2)
#define PS_EMBRYO (3)
*/


#define PS_EMBRYO (-1)
/* The task is either actively running or is able to be run */
#define PS_RUNNING (0)
#define PS_INTERRUPTIBLE (1)
#define PS_UNINTERRUPTIBLE (2)
#define PS_ZOMBIE (3)


#include <schedulers.h>



struct sigaction {
  void (*sa_handler)(int);
  void (*sa_sigaction)(int, long *sigset, void *);
  long sa_mask;
  int sa_flags;
  void (*sa_restorer)(void);
};


namespace sched {
  void block();

  using impl = sched::round_robin;
}  // namespace sched

struct thread_context {
  unsigned long r15;
  unsigned long r14;
  unsigned long r13;
  unsigned long r12;
  unsigned long r11;
  unsigned long rbx;
  unsigned long ebp;  // rbp
  unsigned long eip;  // rip;
};

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
  vec<process::ptr> embryos;
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
  int exit_code = 0;

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
  bool embryonic = false;
  spinlock datalock;

  /* threads stuck in a waitpid() call */
  semaphore waiters = semaphore(0);

  spinlock file_lock;
  map<int, ref<fs::file>> open_files;

  /**
   * exec() - execute a command (implementation for startpid())
   */
  int exec(string &path, vec<string> &argv, vec<string> &envp);

  ref<fs::file> get_fd(int fd);
  int add_fd(ref<fs::file>);

  pid_t create_thread(void *ip, int state);

  // just walks the threads and checks they are all zombie
  bool is_dead(void);

  inline process() {}
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
  int timeslice = 2;
  int priority = 0;
  u64 ticks = 0;
  u64 start_tick = 0;

  // for waitqueues
  struct thread *next = nullptr;
  struct thread *prev = nullptr;
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


struct thread_blocker {
  virtual ~thread_blocker(void) {}
  virtual bool should_unblock(struct thread &t, unsigned long now_us) = 0;
};

struct sleep_blocker final : public thread_blocker {
  // the end time to wait until.
  unsigned long end_us = 0;

  sleep_blocker(unsigned long us_to_sleep);
  virtual ~sleep_blocker(void) {}
  virtual bool should_unblock(struct thread &t, unsigned long now_us);
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

  /* Reference to the kernel stack */
  vec<kernel_stack> stacks;


  thread_blocker *blocker = NULL;

  // Masks are per-thread
  struct {
    unsigned long pending = 0;
    unsigned long mask = 0;
    long handling = -1;
    void *arch_priv = nullptr;
  } sig;


  sched::impl::thread_state sched_state;
  struct thread_sched_info sched;


  struct thread_fpu_info fpu;
  struct thread_statistics stats;

  // bundle locks into a single struct
  struct thread_locks locks;

  // register contexts
  struct thread_context *kern_context;
  reg_t *trap_frame;
  struct thread_waitqueue_info wq;

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


#define BLOCKRES_NORMAL 0
#define BLOCKRES_SIGNAL 1
#define BLOCKRES_DEATH 2

  template <typename T, class... Args>
  [[nodiscard]] int block(Args &&... args) {
    T t(forward<Args>(args)...);
    blocker = &t;

    sched::block();

    // remove the blocker
    blocker = NULL;
    return BLOCKRES_NORMAL;
  }




  void setup_stack(reg_t *);

  bool awaken(bool rude = false);

  // Notify a thread that a signal is available, interrupting it from a
  // waitqueue if there is one
  void interrupt(void);

  // tell the thread to start running at a certain address.
  bool kickoff(void *rip, int state);

  off_t setup_tls(void);

  static thread *lookup(pid_t);
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

  bool init(void);

  bool enabled();

#define SCHED_MLFQ_DEPTH 10
#define PRIORITY_HIGH (SCHED_MLFQ_DEPTH - 1)
#define PRIORITY_IDLE 0

  process &kernel_proc(void);

  void yield(void);

  void do_yield(int status);


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

