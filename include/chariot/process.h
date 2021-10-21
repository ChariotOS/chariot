#pragma once

#include <ck/map.h>
#include <ck/vec.h>
#include <ck/string.h>
#include <fwd.h>
#include <fs.h>

#include <thread.h>
// #include <mm.h>




#define SIGBIT(n) (1llu << (n))

#define RING_KERN 0
#define RING_USER 3


#define PS_EMBRYO (-1)
/* The task is either actively running or is able to be run */
#define PS_RUNNING (0)
#define PS_INTERRUPTIBLE (1)
#define PS_UNINTERRUPTIBLE (2)
#define PS_ZOMBIE (3)



struct sigaction {
  void (*sa_handler)(int);
  void (*sa_sigaction)(int, long *sigset, void *);
  long sa_mask;
  int sa_flags;
  void (*sa_restorer)(void);
};


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
struct Process final : public ck::refcounted<Process> {
  using ptr = ck::ref<Process>;

  long pid;   // process id, and pid of the creator task
  long pgid;  // process group id. Normally the same as the pid itself.
  long ppid;  // process id of the parent process.

  /**
   * array of tasks that are in this process. The order doesn't matter and is
   * probably random.
   */
  spinlock threads_lock;
  ck::vec<ck::ref<Thread>> threads;

  /**
   * when a process spawns a new process, it is an embryo. A process can only
   * pctl() a pid that is in this list. When a task startpid()'s a pid, it will
   * be moved from this list to the children list.
   */
  ck::vec<ck::ref<Process>> children;

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
  ck::ref<Process> parent;

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

  mm::AddressSpace *mm;


  ck::ref<fs::File> executable;

  struct {
    bool exists = false;
    off_t fileoff;
    size_t memsz;
    size_t fsize;
  } tls_info;

  u64 create_tick = 0;
  // The current working directory of the process.
  ck::ref<fs::Node> cwd = nullptr;
  ck::ref<fs::Node> root = nullptr;
  ck::string cwd_string;
  spinlock datalock;

  /* threads stuck in a waitpid() call */
  // semaphore waiters = semaphore(0);

  wait_queue child_wq;

  spinlock file_lock;
  ck::map<int, ck::ref<fs::File>> open_files;


  spinlock futex_lock;
  wait_queue &futex_queue(int *);
  ck::map<off_t, ck::box<wait_queue>> m_futex_queues;

  /**
   * exec() - execute a command (implementation for startpid())
   */
  int exec(ck::string &path, ck::vec<ck::string> &argv, ck::vec<ck::string> &envp);

  ck::ref<fs::File> get_fd(int fd);
  int add_fd(ck::ref<fs::File>);

  long create_thread(void *ip, int state);

  // just walks the threads and checks they are all zombie
  bool is_dead(void);

  void terminate(int signal);

  inline Process() {}
  Process(const Process &) = delete;
  ~Process(void);
};
