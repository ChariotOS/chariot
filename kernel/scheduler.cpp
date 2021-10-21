#include <asm.h>
#include <cpu.h>
#include <lock.h>
#include <ck/map.h>
#include <sched.h>
#include <sleep.h>
#include <syscall.h>
#include <time.h>
#include <wait.h>
#include "printk.h"

#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)


volatile bool did_panic = false;  // printk.h
static bool s_enabled = true;


// copied from
#define MLFQ_NQUEUES 32
// minimum timeslice
#define MLFQ_MIN_RT 1
// how many ticks get added to the timeslice for each priority
#define MLFQ_MUL_RT 1

struct mlfq {
  enum Behavior {
    Good = 0,        // the thread ran for at most as long as it was allowed
    Bad = 1,         // the thread used all of it's timeslice (demote it)
    Unknown = Good,  // Make no decision, effectively Behavior::Good
  };

  struct queue {
    ck::ref<Thread> front = nullptr;
    ck::ref<Thread> back = nullptr;
    void add_task(ck::ref<Thread> tsk) {
      if (front == nullptr) {
        // this is the only thing in the queue
        front = tsk;
        back = tsk;
        tsk->next = nullptr;
        tsk->prev = nullptr;
      } else {
        // insert at the end of the list
        back->next = tsk;
        tsk->next = nullptr;
        tsk->prev = back;
        // the new task is the end
        back = tsk;
      }
    }

    void remove_task(ck::ref<Thread> tsk) {
      if (tsk->next) {
        tsk->next->prev = tsk->prev;
      }
      if (tsk->prev) {
        tsk->prev->next = tsk->next;
      }
      if (back == tsk) back = tsk->prev;
      if (front == tsk) front = tsk->next;
      if (back == nullptr) assert(front == nullptr);
      if (front == nullptr) assert(back == nullptr);

      tsk->next = tsk->prev = nullptr;
    }

    ck::ref<Thread> pick_next(void) {
      ck::ref<Thread> td = nullptr;

      for (ck::ref<Thread> thd = front; thd != nullptr; thd = thd->next) {
        if (thd->get_state() == PS_RUNNING) {
          td = thd;
          remove_task(td);
          break;
        }
      }

      return td;
    }
  };

  int core;                // what cpu core?
  int next_worksteal = 0;  // what core to next steal from
  // is this queue active?
  bool active = false;
  // a lock (must be held with no interrupts)
  spinlock lock;
  // the priority queues.
  //    queues[0] is the highest priority, lowest runtime
  //    queues[MLFQ_NQUEUES-1] is the lowest priority, highest runtime
  mlfq::queue queues[MLFQ_NQUEUES];

  void add(ck::ref<Thread> thd, mlfq::Behavior b = mlfq::Behavior::Unknown) {
    scoped_irqlock l(lock);
    thd->stats.current_cpu = core;

    int old = thd->sched.priority;
    // lower priority is better, so increment it if it's being bad
    if (b == mlfq::Behavior::Bad) {
      thd->sched.good_streak = 0;
      thd->sched.priority++;
    }

    if (b == mlfq::Behavior::Good) {
      // if the thread is not still running (blocked on I/O), imrove their priority
      if (false && thd->get_state() != PS_RUNNING) {
        thd->sched.priority--;
        thd->sched.good_streak = 0;
      } else {
        thd->sched.good_streak++;
        if (thd->sched.good_streak > (thd->sched.priority * 2) + MLFQ_NQUEUES) {
          thd->sched.good_streak = 0;
          thd->sched.priority--;
        }
      }
    }

    if (thd->sched.priority < 0) thd->sched.priority = 0;
    if (thd->sched.priority >= MLFQ_NQUEUES) thd->sched.priority = MLFQ_NQUEUES - 1;


    thd->sched.timeslice = MLFQ_MIN_RT + (thd->sched.priority * 2);
    queues[thd->sched.priority].add_task(thd);

    if (false && old != thd->sched.priority) {
      printk_nolock("\e[2J");

      for (int prio = 0; prio < MLFQ_NQUEUES; prio++) {
        auto &q = queues[prio];
        printk_nolock("prio %02d:", prio);
        for (struct Thread *thd = q.front; thd != NULL; thd = thd->next) {
          printk_nolock(" '%s(good: %d)'", thd->proc.name.get(), thd->sched.good_streak);
        }
        printk_nolock("\n");
      }
      printk_nolock("\n");
    }
  }

  ck::ref<Thread> get_next(void) {
    scoped_irqlock l(lock);
    for (int prio = 0; prio < MLFQ_NQUEUES; prio++) {
      if (ck::ref<Thread> cur = queues[prio].pick_next(); cur != nullptr) return cur;
    }
    return nullptr;
  }



  // steal to another cpu, `thieff`
  ck::ref<Thread> steal(int thief) {
    bool locked = false;
    auto f = lock.try_lock_irqsave(locked);

    if (locked) {
      for (int prio = 0; prio < MLFQ_NQUEUES; prio++) {
        // TODO: make sure it can be taken by the thief
        if (ck::ref<Thread> cur = queues[prio].pick_next(); cur != nullptr) {
          lock.unlock_irqrestore(f);
          // printk_nolock("cpu %d stealing thread %d\n", cpu::current().cpunum, cur->tid);
          return cur;
        }
      }
      lock.unlock_irqrestore(f);
    }
    return nullptr;
  }

  void remove(ck::ref<Thread> thd) {
    scoped_irqlock l(lock);
    assert(thd->stats.current_cpu == core);
    thd->stats.current_cpu = -1;
    queues[thd->sched.priority].remove_task(thd);
  }

  void boost(void) {
    scoped_irqlock l(lock);

    for (int prio = 1; prio < MLFQ_NQUEUES; prio++) {
      auto &q = queues[prio];
      for (ck::ref<Thread> thd = q.front; thd != nullptr; thd = thd->next) {
        q.remove_task(thd);
        thd->sched.priority /= 2;  // get one better :)
        queues[thd->sched.priority].add_task(thd);
      }
    }
  }

  // return the number of runnable threads in this scheduler (incremented and decremented on
  // add/remove)
  int num_runnable(void) { return __atomic_load_n(&m_running, __ATOMIC_ACQUIRE); }

 private:
  int m_running = 0;
};


// each cpu has their own MLFQ. If a core does not have any threads in it's queue, it can steal
// runnable threads from other queues.
static struct mlfq queues[CONFIG_MAX_CPUS];




#ifdef SCHED_DEBUG
#define INFO(fmt, args...) printk("[SCHED] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

extern "C" void context_switch(struct thread_context **, struct thread_context *);



static auto &my_queue(void) { return queues[cpu::current().cpunum]; }


bool sched::init(void) {
  auto &q = my_queue();
  q.core = cpu::current().cpunum;
  q.active = true;
  return true;
}



// work-steal from other cores if they have runnable threads
static ck::ref<Thread> worksteal(void) {
  int nproc = cpu::nproc();
  // printk_nolock("nproc: %d\n", nproc);
  // divide by zero and infinite loop safety
  if (nproc <= 1) return nullptr;

  auto &q = my_queue();
  unsigned int target = q.core;

  do {
    target = (q.next_worksteal++) % nproc;
    // printk_nolock("target: %d\n", target);
  } while (target == q.core);

  if (q.next_worksteal >= nproc) q.next_worksteal = 0;
  if (!queues[target].active) return nullptr;

  ck::ref<Thread> t = queues[target].steal(q.core);



  // printk_nolock("cpu %d stealing from cpu %d : %d\n", q.core, target, t);

  // work steal!
  return t;
}




static struct Thread *get_next_thread(void) {
  ck::ref<Thread> t = my_queue().get_next();

  if (t == nullptr) {
    t = worksteal();
  }

  if (t) {
    t->prev = nullptr;
    t->next = nullptr;
  }

  return t;
}


ck::ref<Thread> pick_next_thread(void) {
  auto &cpu = cpu::current();
  if (!cpu.next_thread) {
    cpu.next_thread = get_next_thread();
  }
  return cpu.next_thread;
}


int sched::add_task(ck::ref<Thread> tsk) {
  auto b = mlfq::Behavior::Good;
  if (tsk->sched.has_run >= tsk->sched.timeslice) b = mlfq::Behavior::Bad;

  my_queue().add(tsk, b);
  return 0;
}

int sched::remove_task(ck::ref<Thread> t) {
  if (t->stats.current_cpu >= 0) {
    // assert(t->stats.current_cpu != -1);  // sanity check
    queues[t->stats.current_cpu].remove(t);
  }
  return 0;
}

static void switch_into(ck::ref<Thread> thd) {
  if (thd->held_lock != NULL) {
    if (!thd->held_lock->try_lock()) return;
  }
  thd->runlock.lock();

  // auto start = time::now_us();

  cpu::current().current_thread = thd;
  cpu::current().next_thread = nullptr;
  thd->state = PS_RUNNING;
  if (thd->proc.ring == RING_USER) arch_restore_fpu(*thd);


  // update the statistics of the thread
  thd->stats.run_count++;
  thd->stats.current_cpu = cpu::current().cpunum;

  thd->sched.has_run = 0;

  // load up the thread's address space
  cpu::switch_vm(thd);

  thd->stats.last_start_cycle = arch_read_timestamp();
  bool ts = time::stabilized();
  long start_us = 0;
  if (ts) {
    start_us = time::now_us();
  }

  // Switch into the thread!
  context_switch(&cpu::current().sched_ctx, thd->kern_context);

  if (thd->proc.ring == RING_USER) arch_save_fpu(*thd);

  if (ts) thd->ktime_us += time::now_us() - start_us;

  // Update the stats afterwards
  cpu::current().current_thread = nullptr;

  // printk_nolock("took %llu us\n", time::now_us() - start);
  thd->runlock.unlock();
  if (thd->held_lock != NULL) thd->held_lock->unlock();
}



sched::yieldres sched::yield(spinlock *held_lock) {
  sched::yieldres r = sched::yieldres::None;
  auto &thd = *curthd;

  thd.held_lock = held_lock;

  // if the old thread is now dead, notify joiners
  if (thd.get_state() == PS_ZOMBIE) {
    thd.joiners.wake_up();
  }

  arch_disable_ints();


  thd.stats.cycles += arch_read_timestamp() - thd.stats.last_start_cycle;
  thd.stats.last_cpu = thd.stats.current_cpu;
  thd.stats.current_cpu = -1;
  context_switch(&thd.kern_context, cpu::current().sched_ctx);

  if (thd.wq.rudely_awoken) r = sched::yieldres::Interrupt;

  arch_enable_ints();
  return r;
}


void sched::set_state(int state) {
  if (curthd == NULL) {
    void *addr = __builtin_extract_return_addr(__builtin_return_address(0));

    panic("NO THREAD from %p!\n", addr);
  }
  auto &thd = *curthd;
  thd.set_state(state);
  // memory barrier!
  asm volatile("" : : : "memory");
}

sched::yieldres sched::do_yield(int st) {
  if (curthd == NULL) printk("NO THREAD!\n");
  sched::set_state(st);
  return sched::yield();
}



// helpful functions wrapping different resulting task states
void sched::block() { sched::do_yield(PS_INTERRUPTIBLE); }
/* Unblock a thread */
void sched::unblock(Thread &thd, bool interrupt) {
#if 0
  if (thd.state != PS_INTERRUPTIBLE) {
    /* Hmm, not sure what to do here. */
    // printk(KERN_WARN "Attempt to wake up thread %d which is not PS_INTERRUPTIBLE\n", thd.tid);
    return;
  }
#endif
  thd.wq.rudely_awoken = interrupt;
  thd.set_state(PS_RUNNING);
  __sync_synchronize();
}

void sched::exit() { do_yield(PS_ZOMBIE); }

void sched::dumb_sleepticks(unsigned long t) {
  auto now = cpu::get_ticks();

  while (cpu::get_ticks() * 10 < now + t) {
    sched::yield();
  }
}


static int idle_task(void *arg) {
  (void)arg;
  /**
   * The way the idle_task works is pretty simple. It is just a context in
   * which it is safe to handle exceptions that might block. If the scheduler
   * itself does a halt wait, it will have to handle an irq in scheduler context
   * which is not allowed to take locks that are shared with other threads. By
   * having a "real thread", though, we pay more cycles per idle loop. This is
   * easier than having to think about being in the scheduler context and fixes
   * a big class of bugs in one go :^).
   */
  while (1) {
    if (did_panic) debug_die();
    /*
     * The loop here is simple. Wait for an interrupt, handle it (implicitly) then
     * yield back to the scheduler if there is a task ready to run.
     */
    arch_enable_ints(); /* just to be sure. */
    arch_halt();
    sched::yield();
  }
}


void sched::run() {
  arch_disable_ints();
  // per-scheduler idle threads do not exist in the scheduler queue.
  auto idle_thread = sched::proc::spawn_kthread("idle task", idle_task, NULL);
  idle_thread->preemptable = true;

  cpu::current().in_sched = true;
  unsigned long has_run = 0;

  while (1) {
    if (did_panic) debug_die();
    if (has_run++ >= 100) {
      has_run = 0;
      my_queue().boost();
    }

    ck::ref<Thread> thd = pick_next_thread();
    cpu::current().next_thread = nullptr;

    if (thd == nullptr) {
      // idle loop when there isn't a task
      idle_thread->sched.timeslice = 1;
      idle_thread->sched.has_run = 0;

      auto start = cpu::get_ticks();
      switch_into(idle_thread);
      auto end = cpu::get_ticks();

      cpu::current().kstat.idle_ticks += end - start;
      continue;
    }

    // printk_nolock("%s %d %d\n", thd->proc.name.get(), thd->tid, thd->ref_count());
    auto start = cpu::get_ticks();
    switch_into(thd);
    auto end = cpu::get_ticks();

    if (thd->proc.ring == RING_KERN) {
      cpu::current().kstat.kernel_ticks += end - start;
    } else {
      cpu::current().kstat.user_ticks += end - start;
    }
    if (thd->get_state() != PS_ZOMBIE) {
      // add the task back to the scheduler
      sched::add_task(thd);
    }
  }
  panic("scheduler should not have gotten back here\n");
}



void sched::handle_tick(u64 ticks) {
  // always check wakeups

  if (!cpu::in_thread()) return;

  //
  check_wakeups();

  // grab the current thread
  auto thd = cpu::thread();
  thd->sched.has_run++;

  /* We don't get preempted if we aren't currently runnable. See wait.cpp for why */
  if (thd->get_state() != PS_RUNNING) return;
  if (thd->preemptable == false) return;

  // yield?
  if (thd->sched.has_run >= thd->sched.timeslice) {
#ifdef CONFIG_PREFETCH_NEXT_THREAD
    pick_next_thread();
#endif
  }
}




#define SIGACT_IGNO 0
#define SIGACT_TERM 1
#define SIGACT_STOP 2
#define SIGACT_CONT 3


static int default_signal_action(int signal) {
  ASSERT(signal >= 0 && signal < 64);

  switch (signal) {
    case SIGHUP:
    case SIGINT:
    case SIGKILL:
    case SIGPIPE:
    case SIGALRM:
    case SIGUSR1:
    case SIGUSR2:
    case SIGVTALRM:
    // case SIGSTKFLT:
    case SIGIO:
    case SIGPROF:
    case SIGTERM:
      return SIGACT_TERM;
    case SIGCHLD:
    case SIGURG:
    case SIGWINCH:
      // case SIGINFO:
      return SIGACT_IGNO;
    case SIGQUIT:
    case SIGILL:
    case SIGTRAP:
    case SIGABRT:
    case SIGBUS:
    case SIGFPE:
    case SIGSEGV:
    case SIGXCPU:
    case SIGXFSZ:
    case SIGSYS:
      return SIGACT_TERM;
    case SIGCONT:
      return SIGACT_CONT;
    case SIGSTOP:
    case SIGTSTP:
    case SIGTTIN:
    case SIGTTOU:
      return SIGACT_STOP;
  }

  return SIGACT_TERM;
}



int sched::claim_next_signal(int &sig, void *&handler) {
  if (cpu::in_thread()) {
    int sig_to_handle = -1;

    if (curthd->sig.handling == -1) {
      while (1) {
        sig_to_handle = -1;
        if (curthd->sig.pending != 0) {
          for (int i = 0; i < 63; i++) {
            if ((curthd->sig.pending & SIGBIT(i)) != 0) {
              if (curthd->sig.mask & SIGBIT(i)) {
                // Mark this signal as handled.
                curthd->sig.pending &= ~SIGBIT(i);
                sig_to_handle = i;
                break;
              } else if (default_signal_action(i) == SIGACT_TERM) {
                curproc->terminate(i);
              }
            }
          }
        }


        if (sig_to_handle != -1) {
          if (!cpu::in_thread()) {
            panic("not in cpu when getting signal %d\n", sig);
          }

          assert(curthd->sig.handling == -1);


          auto &action = curproc->sig.handlers[sig];

          if (sig == SIGSTOP) {
            printk("TODO: SIGSTOP\n");
            return -1;
          }

          if (sig == SIGCONT) {
            printk("TODO: SIGCONT\n");
            // resume_from_stopped();
          }

          if (action.sa_handler == SIG_DFL) {
            // handle the default action
            switch (default_signal_action(sig)) {
              case SIGACT_STOP:
                printk("TODO: SIGACT_STOP!\n");
                return -1;
              case SIGACT_TERM:
                curproc->terminate(sig);
                return -1;
              case SIGACT_IGNO:
                return -1;
              case SIGACT_CONT:
                printk("TODO: SIGACT_CONT!\n");
                return -1;
            }
          }

          sig = sig_to_handle;
          curthd->sig.handling = sig; /* TODO this might need some more signal-speicifc logic */
          handler = (void *)action.sa_handler;


          return 0;
        } else {
          break;
        }
      }
    }
  }
  return -1;
}


void sched::before_iret(bool userspace) {
  auto &c = cpu::current();


  if (!cpu::in_thread()) return;

  auto thd = curthd;

  // exit via the scheduler if the task should die.
  if (thd->should_die) {
    thd = nullptr;
    sched::exit();
  }

  if (time::stabilized()) thd->last_start_utime_us = time::now_us();

  // if its not running,
  if (thd->state != PS_RUNNING) return;

  bool out_of_time = thd->sched.has_run >= thd->sched.timeslice;
  if (out_of_time || cpu::current().next_thread != nullptr || c.woke_someone_up) {
    c.woke_someone_up = false;
    thd = nullptr;
    sched::yield();
  }
}
