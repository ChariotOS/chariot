// A single header file for all things task and scheduler related

#pragma once

#include <func.h>
#include <map.h>
#include <ptr.h>
#include <string.h>
#include <vec.h>
#include <mm.h>

#include <signals.h>
#define SIGBIT(n) (1 << (n))

#define RING_KERN 0
#define RING_USER 3

#define PS_UNRUNNABLE (-1)
#define PS_RUNNABLE (0)
#define PS_ZOMBIE (1)
#define PS_BLOCKED (2)
#define PS_EMBRYO (3)

struct regs {
  unsigned long rax;
  unsigned long rbx;
  unsigned long rcx;
  unsigned long rdx;
  unsigned long rbp;
  unsigned long rsi;
  unsigned long rdi;
  unsigned long r8;
  unsigned long r9;
  unsigned long r10;
  unsigned long r11;
  unsigned long r12;
  unsigned long r13;
  unsigned long r14;
  unsigned long r15;

  unsigned long trapno;
  unsigned long err;

  unsigned long eip;  // rip
  unsigned long cs;
  unsigned long eflags;  // rflags
  unsigned long esp;     // rsp
  unsigned long ds;      // ss
};

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
    u64 mask;
    u64 pending;
    void *handlers[64] = {0};
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

  u64 create_tick = 0;
  // The current working directory of the process.
  fs::inode *cwd = nullptr;
  bool embryonic = false;
  spinlock datalock;

  /* threads stuck in a waitpid() call */
  waitqueue waiters;

  spinlock file_lock = spinlock("task.file_lock");
  map<int, ref<fs::filedesc>> open_files;

  /**
   * exec() - execute a command (implementation for startpid())
   */
  int exec(string &path, vec<string> &argv, vec<string> &envp);

  ref<fs::filedesc> get_fd(int fd);
  int add_fd(ref<fs::filedesc>);

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
};

struct thread_sched_info {
  int timeslice = 2;
  int priority = 0;
  u64 ticks = 0;
  u64 start_tick = 0;
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
  struct thread *next;
  struct thread *prev;
  waitqueue *current_wq = NULL;
};

struct thread final {
  pid_t tid;  // unique thread id
  pid_t pid;  // process id. If this thread is the main thread, tid == pid

  // A reference to the process this thread belongs to.
  process &proc;

  volatile int state;

  int exit_code;

  /* Reference to the kernel stack */
  long stack_size;
  void *stack;

  struct thread_sched_info sched;
  struct thread_fpu_info fpu;
  struct thread_statistics stats;

  // bundle locks into a single struct
  struct thread_locks locks;

  // register contexts
  struct thread_context *kern_context;
  struct regs *trap_frame;
  struct thread_waitqueue_info wq;

  union /* flags */ {
    u64 flags = 0;

    struct /* bitmask */ {
      unsigned kthread : 1;
      unsigned should_die : 1;  // the thread needs to be torn down, must not
                                // return to userspace
      unsigned kern_idle : 1;   // the thread is a kernel idle thread
      unsigned exited : 1;  // the thread has exited, it's safe to delete when
                            // reaped by the parent or another thread
    };
  };

  /**
   * Awaken the thread from it's waitqueue. Being rudely awoken means that the
   * waitqueue may not have been completed. A thread would be rudely awoken when
   * the process decides to exit, or a fault occurs.
   */
  bool awaken(bool rudely = false);

  // tell the thread to start running at a certain address.
  bool kickoff(void *rip, int state);

  static thread *lookup(pid_t);
  static bool teardown(thread *);

  /**
   * Do not use this API, go through sched::proc::* to allocate and deallocate
   * processes and threads.
   */
  thread(pid_t tid, struct process &);
  thread(const thread &) = delete;  // no copy
  ~thread(void);
};

namespace sched {

bool init(void);

bool enabled();

#define SCHED_MLFQ_DEPTH 10
#define PRIORITY_HIGH (SCHED_MLFQ_DEPTH - 1)
#define PRIORITY_IDLE 0

process &kernel_proc(void);

void yield(void);

void do_yield(int status);

void block();

// does not return
void run(void);

void handle_tick(u64 tick);

// force the process to exit, (yield with different state)
void exit();

void play_tone(int frq, int dur);
int remove_task(struct thread *t);
int add_task(struct thread *);
void beep();

// called before dropping back into user space. This is needed
// when a thread should not return to userspace because it must
// die or something else
void before_iret(bool userspace);

namespace proc {
struct spawn_options {
  int ring = RING_USER;
};

#define SPAWN_KERN (1 << 0)
// #define SPAWN_VFORK (1 << 1)
process::ptr spawn_process(struct process *parent, int flags);

// get the kernel process (creating if it doesnt exist
struct process *kproc(void);

pid_t create_kthread(int (*func)(void *), void *arg = NULL);

void dump_table();

// spawn init (pid 1) and try to execute each of the paths in order, stopping on
// the first success. Returns -1
pid_t spawn_init(vec<string> &paths);

/* remove a process from the ptable */
bool ptable_remove(pid_t);


bool send_signal(pid_t, int sig);

int reap(process::ptr);


int do_waitpid(pid_t, int &status, int options);

};  // namespace proc
}  // namespace sched

