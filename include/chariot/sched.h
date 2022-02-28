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
#include <fwd.h>




// #include <schedulers.h>

namespace sched {
  void block();
  void unblock(Thread &, bool interrupt = false);



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

  int remove_task(ck::ref<Thread> t);
  int add_task(ck::ref<Thread> t);

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
    ck::ref<Process> spawn_process(Process *parent, int flags);

    // get the kernel process (creating if it doesnt exist
    Process *kproc(void);

    long create_kthread(const char *name, int (*func)(void *), void *arg = NULL);

    ck::ref<Thread> spawn_kthread(const char *name, int (*func)(void *), void *arg = NULL);

    void dump_table();

    // spawn init (pid 1) and try to execute each of the paths in order,
    // stopping on the first success. Returns -1
    long spawn_init(ck::vec<ck::string> &paths);

    /* remove a process from the ptable */
    bool ptable_remove(long);

    int send_signal(long, int sig);

    int reap(ck::ref<Process>);

    int do_waitpid(long, int &status, int options);

    // this takes a lock over the process table while iterating
    // Return false from the callback to stop.
    void in_pgrp(long pgid, ck::func<bool(Process &)>);


  };  // namespace proc

}  // namespace sched

// #endif
