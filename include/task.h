#pragma once

#include <atom.h>
#include <fs/vfs.h>
#include <func.h>
#include <lock.h>
#include <map.h>
#include <types.h>
#include <vm.h>

#define BIT(n) (1 << (n))

struct task_regs {
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

struct task_context {
  unsigned long r15;
  unsigned long r14;
  unsigned long r13;
  unsigned long r12;
  unsigned long r11;
  unsigned long rbx;
  unsigned long ebp;  // rbp
  unsigned long eip;  // rip;
};

using pid_t = long;
using gid_t = long;

#define SPWN_FORK BIT(0)

/*
 * Per process flags
 *
 * These are mostly stollen from linux in include/linux/sched.h
 */
#define PF_IDLE BIT(0)     // is an idle thread
#define PF_EXITING BIT(1)  // in the process of exiting
#define PF_KTHREAD BIT(2)  // is a kernel thread
#define PF_MAIN BIT(3)     // this task is the main task for a process

// Process states
#define PS_UNRUNNABLE (-1)
#define PS_RUNNABLE (0)
#define PS_ZOMBIE (1)
#define PS_BLOCKED (2)
#define PS_EMBRYO (3)

struct fd_flags {
  inline operator bool() { return !!fd; }
  void clear();
  int flags;
  ref<fs::filedesc> fd;
};

#define TEV_PROC_DIE 1
#define TEV_TASK_DIE 2

/**
 * a child event is created whenever any of the TEV_* events occur and is added
 * to a process when that happens
 */
struct child_event {
  int type;
  int id;  // pid or tid, depending on event
};

struct task_process {
 public: /* I know, all this stuff is public, I'm a bad OOP programmer but its
            so the syscall interface can just access all the stuff in here. Also
            getters and setters suck. */
  int pid;  // obviously the process id

  // per-process flags (PF_*)
  unsigned long flags = 0;
  int spawn_flags = 0;
  // execution ring (0 for kernel, 3 for user)
  int ring;

  long uid, euid;
  long gid, egid;

  uint64_t create_tick = 0;

  /* address space information */
  vm::addr_space mm;

  // the main executable of this process. To load on first run
  fs::filedesc executable;

  /* cwd - current working directory
   */
  fs::inode *cwd = NULL;

  // every process has a name
  string name;
  string command_path;

  // procs have arguments and enviroments.
  vec<string> args;
  vec<string> env;

  // vector of task ids
  vec<int> tasks;

  // processes that have been spawned but aren't ready yet
  vec<pid_t> nursery;

  pid_t search_nursery(pid_t);

  struct task_process *parent;

  spinlock proc_lock = spinlock("proc.proc_lock");

  // create a thread in the task_process
  int create_task(int (*fn)(void *), int flags, void *arg,
                  int state = PS_RUNNABLE);

  static struct task_process *spawn(pid_t parent, int &error);
  static struct task_process *lookup(int pid);

  // initialize the kernel ``process''
  static task_process *kproc_init(void);

  task_process();

  spinlock file_lock = spinlock("task.file_lock");
  map<int, fd_flags> open_files;

  // syscall interfaces
  int open(const char *path, int flags, int mode);

  int read(int fd, void *dst, size_t sz);
  int write(int fd, void *data, size_t sz);
  int close(int fd);

  // if newfd == -1, acts as dup(a), else act as dup2(a, b)
  int do_dup(int oldfd, int newfd);

  // dump all the processes to the console for debug reasons
  static void dump(void);

  inline ref<fs::filedesc> get_fd(int fd) {
    file_lock.lock();
    ref<fs::filedesc> n;
    if (open_files.contains(fd)) {
      n = open_files[fd].fd;
    }
    file_lock.unlock();
    return n;
  }
};

/**
 * task - a schedulable entity in the kernel
 */
struct task final {
  /* task id */
  int tid;
  /* process id */
  int pid;

  /* -1 unrunnable, 0 runnable, >0 stopped */
  volatile long state;

  long exit_code;

  /* kenrel stack */
  long stack_size;
  void *stack;
  // where the FPU info is saved
  void *fpu_state;

  unsigned long syscall_count = 0;
  unsigned long run_count = 0;

  // the current priority of this task
  int priority;
  bool is_idle_thread = false;

  bool fpu_initialized = false;

  // if the thread should die instead of return to userspace.
  // (for thread teardown)
  bool should_die = false;

  /* per-task flasg (uses PF_* macros)*/
  unsigned int flags;

  struct task_process *proc;

  /* the current cpu this task is running on */
  int current_cpu = -1;
  /* the previous cpu */
  int last_cpu = -1;

  // used when an action requires ownership of this task
  spinlock task_lock = spinlock("task.task_lock");
  // so only one scheduler can run the task at a time (TODO: just work around
  // this possibility)
  spinlock run_lock = spinlock("task.run_lock");

  struct task_context *ctx;
  struct task_regs *tf;

  // how many times the task has been ran
  unsigned long ticks = 0;
  unsigned long start_tick;  // last time it has been started
  int timeslice = 2;

  // for the scheduler's intrusive runqueue
  struct task *next = nullptr;
  struct task *prev = nullptr;

  /**
   * for waitqueue's intrusive ... queue
   */
  // how many *things* this thread is waiting on
  unsigned waiting_on = 0;
  int wait_flags = 0;
  bool rudely_awoken = false;
  // intrusive list for the waitqueue. It's the first if wq_prev == NULL
  struct task *wq_next;
  struct task *wq_prev;
  waitqueue *current_wq = NULL;



  // awaken the task from a blocked state, returning if it woke.
  // rudely means that it is being awoken not by waitqueue::notify, but by
  // something else (like a task teardown)
  bool awaken(bool rudely = 0);
  static struct task *lookup(int tid);
  // protected constructor - must use ::create
  task(struct task_process *);
  ~task(void);

  void exit(int code);
};
