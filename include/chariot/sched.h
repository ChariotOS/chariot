// A single header file for all things task and scheduler related

#pragma once


// #ifdef __cplusplus

#include "ptr.h"

#include <ck/func.h>
#include <ck/map.h>
#include <mm.h>
#include <sem.h>
#include <signals.h>
#include <ck/string.h>
#include <ck/vec.h>


#include <thread.h>




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
struct process final : public ck::refcounted<struct process> {
  using ptr = ck::ref<process>;

  long pid;   // process id, and pid of the creator task
  long pgid;  // process group id. Normally the same as the pid itself.
  long ppid;  // process id of the parent process.

  /**
   * array of tasks that are in this process. The order doesn't matter and is
   * probably random.
   */
  spinlock threads_lock;
  ck::vec<ck::ref<thread>> threads;

  /**
   * when a process spawns a new process, it is an embryo. A process can only
   * pctl() a pid that is in this list. When a task startpid()'s a pid, it will
   * be moved from this list to the children list.
   */
  ck::vec<process::ptr> children;

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
  ck::ref<process> parent;

  /**
   * Information about the user associated with this process. Inherited from
   * parent on spawn.
   */
  struct process_user_info user;

  /**
   * Metadata like name, arguments and envionment variables are converted
   * to kernel structures and stored in the process information.
   */
  ck::string name;
  ck::vec<ck::string> args;
  ck::vec<ck::string> env;

  bool exited = 0;
  uint8_t exit_code = 0;
  uint8_t exit_signal = 0;

  mm::space *mm;


  ck::ref<fs::file> executable;

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
  ck::string cwd_string;
  spinlock datalock;

  /* threads stuck in a waitpid() call */
  // semaphore waiters = semaphore(0);

  wait_queue child_wq;

  spinlock file_lock;
  ck::map<int, ck::ref<fs::file>> open_files;


  spinlock futex_lock;
  wait_queue &futex_queue(int *);
  ck::map<off_t, ck::box<wait_queue>> m_futex_queues;

  /**
   * exec() - execute a command (implementation for startpid())
   */
  int exec(ck::string &path, ck::vec<ck::string> &argv, ck::vec<ck::string> &envp);

  ck::ref<fs::file> get_fd(int fd);
  int add_fd(ck::ref<fs::file>);

  long create_thread(void *ip, int state);

  // just walks the threads and checks they are all zombie
  bool is_dead(void);

  void terminate(int signal);

  inline process() {}
  process(const process &) = delete;
  ~process(void);
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

  int remove_task(ck::ref<thread> t);
  int add_task(ck::ref<thread> t);

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

    long create_kthread(const char *name, int (*func)(void *), void *arg = NULL);

    ck::ref<struct thread> spawn_kthread(const char *name, int (*func)(void *), void *arg = NULL);

    void dump_table();

    // spawn init (pid 1) and try to execute each of the paths in order,
    // stopping on the first success. Returns -1
    long spawn_init(ck::vec<ck::string> &paths);

    /* remove a process from the ptable */
    bool ptable_remove(long);

    int send_signal(long, int sig);

    int reap(process::ptr);

    int do_waitpid(long, int &status, int options);

    // this takes a lock over the process table while iterating
    // Return false from the callback to stop.
    void in_pgrp(long pgid, ck::func<bool(struct process &)>);


  };  // namespace proc

}  // namespace sched

// #endif
