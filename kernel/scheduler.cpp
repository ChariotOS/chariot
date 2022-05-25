#include <asm.h>
#include <cpu.h>
#include <lock.h>
#include <ck/map.h>
#include <sched.h>
#include <sleep.h>
#include <syscall.h>
#include <time.h>
#include <wait.h>
#include <printf.h>
#include <realtime.h>

#ifdef CONFIG_RISCV
#include <riscv/arch.h>
#endif

#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

volatile long monitor_tid = 0;
volatile bool did_panic = false;  // printk.h
static bool s_enabled = true;


// #define SCHED_DO_DEBUG
#ifdef SCHED_DO_DEBUG
#define SCHED_DEBUG printf_nolock
#else
#define SCHED_DEBUG(...)
#endif

int Thread::make_runnable(int cpu, bool admit) {
  // Which core do we want to run on
  cpu::Core *target_core = nullptr;

  // cpu of -1 means "self"
  if (cpu == RT_CORE_SELF) {
    target_core = &core();
    cpu = target_core->id;
  } else if (cpu == RT_CORE_ANY) {
    // Pick a random core. If that core doesn't allow the task,
    int nprocs = cpu::nproc();
    unsigned start = rand() % nprocs;
    for (int i = 0; i < nprocs; i++) {
      int target = (start + i) % nprocs;
      // try to admit to the core
      auto res = make_runnable(target, admit);
      if (res >= 0) {
        // printf("running on %d\n", target);
        return res;
      }
    }
    // We couldn't find a core to run it on.
    return -ENOENT;
  } else {
    target_core = cpu::get(cpu);
    if (target_core == nullptr) {
      SCHED_DEBUG(KERN_WARN "Could not find cpu %d during rt::make_runnable. Defaulting to self\n", cpu);
      target_core = &core();
    }
  }


  // if we didn't find a core, report that back to the caller
  if (target_core == nullptr) return -ENOENT;

  auto &s = target_core->local_scheduler;
  // grab a scoped lock
  auto lock = s.lock();

  if (admit) {
    bool admitted = s.admit(this, time::now_us());
    // if we failed to admit, warn
    if (admitted == false) {
      // TODO: remove this
      SCHED_DEBUG(KERN_WARN "Failed to admit thread\n");
      return -1;
    } else {
      // SCHED_DEBUG("%d admit %d\n", cpu, this->tid);
    }
  }


  if (constraint().type == rt::APERIODIC) {
    s.aperiodic.enqueue(this);
    this->rt_status = rt::ADMITTED;
  } else {
    printf(KERN_WARN "Cannot handle non-aperiodic tasks yet\n");
    return -ENOTIMPL;
  }

  return 0;
}



rt::Scheduler::Scheduler(cpu::Core &core) : m_core(core) {}


bool rt::Scheduler::admit(Thread *task, uint64_t now) {
  const auto &constraint = task->constraint();

  if (constraint.type == ConstraintType::APERIODIC) {
    // Always admit aperiodic tasks
    task->reset_state();
    task->reset_stats();

    task->scheduler = this;
    task->deadline = constraint.aperiodic.priority;

    return true;
  }

  return false;
}

int rt::Scheduler::dequeue(Thread *task) {
  printf_nolock("DEQUEUE %d\n", task->tid);
  assert(task->scheduler == this);
  if (task->current_queue != NULL) {
    task->current_queue->remove(task);
  }

  return 0;
}

bool rt::Scheduler::reschedule(void) {
  // TODO: do other stuff
  Thread *res = NULL;

  auto l = lock();
  if (next_thread) return true;


  // Go through all the queues, looking for a task to run
  if (res == NULL) {
    res = runnable.dequeue();
  }

  if (res == NULL) {
    // printf("---------------------\n");
    // aperiodic.dump("before");
    res = aperiodic.dequeue();
    // aperiodic.dump(" after");
    // printf("\n");
  }


  // We've decided on res to run next. It no longer sits in a queue, but we will place
  // it on next_thread for ::claim to eventually pop
  if (res != NULL) {
    this->next_thread = res;
    return true;
  }
  return false;
}


void rt::Scheduler::pump_sized_tasks(Thread *next) {
  //
}

ck::ref<Thread> rt::Scheduler::claim(void) {
  auto l = lock();
  auto t = next_thread;
  if (t) {
    next_thread = nullptr;
    // now that we've claimed it, it no longer sits on a queue anywhere
    t->scheduler = nullptr;
  }
  return t;
}


void rt::Scheduler::kick(void) {
  // kicks must be from remote cores.
  if (core_id() != this->core().id) {
    cpu::xcall(
        this->core().id,
        [](void *arg) {
          auto targ = static_cast<rt::Scheduler *>(arg);
          targ->in_kick = true;
        },
        this);
    // apic_ipi(per_cpu_get(apic), nk_get_nautilus_info()->sys.cpus[cpu]->lapic_id, APIC_NULL_KICK_VEC);
  } else {
    // we do not reschedule here since
    // we do not know if it is safe to do so
  }
}


scoped_irqlock rt::Scheduler::lock(void) { return m_lock; }

scoped_irqlock rt::local_lock(void) { return core().local_scheduler.lock(); }


void rt::PriorityQueue::enqueue(Thread *task) {
  assert(task->current_queue == NULL);
  task->current_queue = this;
  m_size++;
  // place them in the queue
  rb_insert(m_root, &task->prio_node, [&](struct rb_node *o) {
    auto *other = rb_entry(o, Thread, prio_node);
    long result = (long)task->deadline - (long)other->deadline;
    if (result < 0) {
      // if `task` has an earlier deadline, put it left
      return RB_INSERT_GO_LEFT;
    } else if (result >= 0) {
      // if `task` has greater deadline, go right
      // if `task` has equal deadline, don't jump in line
      return RB_INSERT_GO_RIGHT;
    }
    return RB_INSERT_GO_HERE;
  });
}

Thread *rt::PriorityQueue::peek(void) {
  // save some time
  if (m_size == 0) return nullptr;
  // get the first in the tree (which is sorted by prio/deadline)
  auto *first_node = rb_first(&m_root);
  if (first_node == nullptr) return nullptr;
  // grab the task and return it
  Thread *task = rb_entry(first_node, Thread, prio_node);
  return task;
}


void rt::PriorityQueue::remove(Thread *task) {
  if (task == nullptr) return;
  assert(task->current_queue == this);
  // This task must be in this queue (we assume there is one type of queue per
  // rt::Scheduler)
  rb_erase(&task->prio_node, &m_root);
  m_size--;
  task->current_queue = NULL;
}

void rt::PriorityQueue::dump(const char *msg) {
  SCHED_DEBUG("%s: ");
  Thread *n, *node;

  rbtree_postorder_for_each_entry_safe(node, n, &m_root, prio_node) {
    // just log the tid
    SCHED_DEBUG(" %d", node->tid);
  }
  SCHED_DEBUG("\n");
}

Thread *rt::TaskQueue::dequeue(void) {
  auto next = peek();
  if (next != NULL) {
    remove(next);
  }
  return next;
}

size_t size_slow(struct list_head *head) {
  size_t c = 0;
  Thread *ent;
  list_for_each_entry(ent, head, queue_node) { c++; }
  return c;
}


void rt::Queue::enqueue(Thread *task) {
  assert(task->current_queue == NULL);
  task->current_queue = this;
  m_list.add_tail(&task->queue_node);
  m_size++;
  // assert(m_size == size_slow(&m_list));
}

Thread *rt::Queue::peek(void) {
  auto next = m_list.next;
  if (next == &m_list) return nullptr;
  auto *task = list_entry(next, Thread, queue_node);
  return task;
}

void rt::Queue::remove(Thread *task) {
  if (task == NULL) return;
  assert(task->current_queue == this);
  task->queue_node.del_init();
  task->current_queue = NULL;
  m_size--;
  // assert(m_size == size_slow(&m_list));
}


void rt::Queue::dump(const char *msg) {
  // return;
  SCHED_DEBUG("%s: ");
  Thread *pos = NULL;
  list_for_each_entry(pos, &m_list, queue_node) { SCHED_DEBUG(" %d", pos->tid); }
  SCHED_DEBUG("\n");
}


void sched::dump(void) {
  // return;
  printf("------ Scheduler Dump ------\n");
  /*
for (int i = 0; i < CONFIG_MAX_CPUS; i++) {
if (queues[i].active == false) continue;

auto &q = queues[i];
auto *core = cpu::get(q.core);
auto &current = core->current_thread;
// printf("Core %d. current=%d\n", q.core, current ? current->tid : -1);
for (int d = 0; d < MLFQ_NQUEUES; d++) {
auto &m = q.queues[d];
printf("  - %2d: ", d);
for (auto cur = m.front; cur != nullptr; cur = cur->next) {
  printf(" %4d", cur->tid);
}
printf("\n");
}
}
  */
}




extern "C" void context_switch(struct ThreadContext **, struct ThreadContext *);




bool sched::init(void) { return true; }


int sched::add_task(ck::ref<Thread> tsk) {
  int res = tsk->make_runnable(RT_CORE_SELF, true);
  SCHED_DEBUG("add task %d = %d\n", tsk->tid, res);
  return res;
}

int sched::remove_task(ck::ref<Thread> t) {
  t->remove_from_scheduler();
  return 0;
}

static void switch_into(ck::ref<Thread> thd) { thd->run(); }



sched::yieldres sched::yield() {
  if (!arch_irqs_enabled()) return sched::yieldres::None;
  sched::yieldres r = sched::yieldres::None;
  auto &thd = *curthd;

  arch_disable_ints();
  barrier();


  thd.stats.cycles += arch_read_timestamp() - thd.stats.last_start_cycle;
  thd.stats.last_cpu = thd.stats.current_cpu;
  thd.stats.current_cpu = -1;
  barrier();
  context_switch(&thd.kern_context, cpu::current().sched_ctx);
  barrier();

  if (thd.rudely_awoken) {
    r = sched::yieldres::Interrupt;
  }
  thd.rudely_awoken = false;

  barrier();
  arch_enable_ints();
  return r;
}


void sched::set_state(int state) {
  if (curthd == NULL) {
    void *addr = __builtin_extract_return_addr(__builtin_return_address(0));

    // panic("NO THREAD from %p!\n", addr);
  }
  auto &thd = *curthd;
  thd.set_state(state);

  barrier();
}

sched::yieldres sched::do_yield(int st) {
  if (curthd == NULL) printf("NO THREAD!\n");
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
    // printf(KERN_WARN "Attempt to wake up thread %d which is not PS_INTERRUPTIBLE\n", thd.tid);
    return;
  }
#endif

  // if () {}

  // assert(thd.current_scheduler() == NULL);
  thd.rudely_awoken = interrupt;
  thd.set_state(PS_RUNNING);
  __sync_synchronize();
  sched::add_task(&thd);
}

void sched::exit() {
  curthd->exit();
  yield();
}

#include <rcu.h>

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
    // if (did_panic) debug_die();
    /*
     * The loop here is simple. Wait for an interrupt, handle it (implicitly) then
     * yield back to the scheduler if there is a task ready to run.
     */
    arch_enable_ints(); /* just to be sure. */
    arch_halt();
    sched::yield();
  }
}



static int active_cores = 0;
static int barrier_waiters = 0;

void sched_barrier() {
  if (__atomic_add_fetch(&barrier_waiters, 1, __ATOMIC_ACQUIRE) == active_cores) {
    __atomic_sub_fetch(&barrier_waiters, active_cores, __ATOMIC_RELEASE);
    return;
  }

  while (__atomic_load_n(&barrier_waiters, __ATOMIC_RELAXED) != 0) {
    arch_relax();
  }
}


void sched::run() {
  __atomic_fetch_add(&active_cores, 1, __ATOMIC_SEQ_CST);
  arch_disable_ints();
  // per-scheduler idle threads do not exist in the scheduler queue.
  ck::ref<Thread> idle_thread = sched::proc::spawn_kthread("idle task", idle_task, NULL);
  idle_thread->preemptable = true;
  core().in_sched = true;



  rt::Scheduler &sched = core().local_scheduler;

  while (1) {
    sched.reschedule();
    ck::ref<Thread> thd = sched.claim();

    if (did_panic && thd != nullptr) {
      // only run kernel threads, and the monitor thread.
      if (thd->tid != monitor_tid && thd->proc.ring == RING_USER) {
        sched::add_task(thd);
        thd = nullptr;
      }
    }

    // If there isn't a thread to run... Schedule the idle thread
    if (thd == nullptr) {
      auto start = cpu::get_ticks();
      // printf_nolock("idle\n");
      idle_thread->run();
      auto end = cpu::get_ticks();

      cpu::current().kstat.idle_ticks += end - start;
      continue;
    }

    auto start = cpu::get_ticks();
    auto state_before = thd->get_state();
    if (state_before == PS_RUNNING) {
      thd->run();
    } else {
      SCHED_DEBUG("State was not running! Was %d\n", state_before);
    }

    auto state_after = thd->get_state();

    auto end = cpu::get_ticks();
    auto ran = end - start;
    if (thd) {
      switch (thd->proc.ring) {
        case RING_KERN:
          cpu::current().kstat.kernel_ticks += ran;
          break;

        case RING_USER:
          cpu::current().kstat.user_ticks += ran;
          break;
      }

      // if the old thread is now dead, notify joiners
      if (thd->should_die) {
        SCHED_DEBUG("Should die.\n");
        scoped_irqlock l(thd->joinlock);
        thd->set_state(PS_ZOMBIE);
        thd->joiners.wake_up_all();
      } else if (state_after == PS_RUNNING) {
        // SCHED_DEBUG("Still runnable\n");
        thd->make_runnable(RT_CORE_SELF, true);
      } else {
        SCHED_DEBUG("Was blocked.\n");
      }
    }
  }
  panic("scheduler should not have gotten back here\n");
}



void sched::handle_tick(u64 ticks) {
  if (!core().in_sched) return;
  if (!cpu::in_thread()) return;

  //
  check_wakeups();

  // grab the current thread
  auto thd = cpu::thread();
  thd->ticks_ran++;

  /* We don't get preempted if we aren't currently runnable. See wait.cpp for why */
  if (thd->get_state() != PS_RUNNING) return;
	// if the current thread is not preemptable, or we are in an RCU reader
	// section, don't reschedule
  if (thd->preemptable == false || core().preempt_count != 0) return;

  // ask the scheduler if there's anything to switch to
  if (core().local_scheduler.reschedule()) {
    // !
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
            printf("TODO: SIGSTOP\n");
            return -1;
          }

          if (sig == SIGCONT) {
            printf("TODO: SIGCONT\n");
            // resume_from_stopped();
          }

          if (action.sa_handler == SIG_DFL) {
            // handle the default action
            switch (default_signal_action(sig)) {
              case SIGACT_STOP:
                printf("TODO: SIGACT_STOP!\n");
                return -1;
              case SIGACT_TERM:
                curproc->terminate(sig);
                return -1;
              case SIGACT_IGNO:
                return -1;
              case SIGACT_CONT:
                printf("TODO: SIGACT_CONT!\n");
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

  // exit via the scheduler if the task should die, and the irq depth is 1 (we
  // would return back to the thread)
  if (thd->should_die) {
    c.woke_someone_up = false;
    sched::yield();
  }

  // if (time::stabilized()) thd->last_start_utime_us = time::now_us();

  // if its not running, it is setting up a waitqueue or exiting, so we don't
  // want to screw that up by prematurely yielding.
  if (thd->state != PS_RUNNING) {
    return;
  }

  bool out_of_time = thd->ticks_ran >= thd->timeslice;
  if (out_of_time || c.local_scheduler.next_thread != nullptr || c.woke_someone_up) {
    c.woke_someone_up = false;
    thd = nullptr;
    sched::yield();
  }
}



struct send_signal_arg {
  long p;
  int sig;
};

static void send_signal_xcall(void *arg) {
  auto a = (struct send_signal_arg *)arg;
  // printf("core %d running %d when signal t:%d p:%d is sent to %ld\n", core_id(), curthd->tid, curthd->pid, a->sig, a->p);
  long target = a->p;

  if (target < 0) {
    long pgid = -target;
    if (pgid == curproc->pgid) {
      printf("core %d is running the target pgid\n", core_id());
    }
  } else if (target == curthd->pid) {
    printf("core %d is running the target pid\n", core_id());
  } else if (target == curthd->tid) {
    printf("core %d is running the target tid\n", core_id());
  }
}

// kernel/process.cpp
extern ck::ref<Process> pid_lookup(long pid);

int sched::proc::send_signal(long p, int sig) {
  printf("send signal %d to %d\n", sig, p);


  // handle self signal
  if (curthd->tid == p) {
    printf("early ret\n");
    curthd->send_signal(sig);
    return 0;
  }

  struct send_signal_arg arg;
  arg.p = p;
  arg.sig = sig;
  cpu::xcall_all(send_signal_xcall, &arg);


  // TODO: handle process group signals
  if (p < 0) {
    int err = -ESRCH;
    long pgid = -p;
    ck::vec<long> targs;
    sched::proc::in_pgrp(pgid, [&](struct Process &p) -> bool {
      targs.push(p.pid);
      return true;
    });

    for (long pid : targs)
      sched::proc::send_signal(pid, sig);

    return 0;
  }

  if (sig < 0 || sig >= 64) {
    return -EINVAL;
  }

  int err = -ESRCH;
  {
    auto targ = pid_lookup(p);

    if (targ) {
      // find a thread
      for (auto thd : targ->threads) {
        if (thd->send_signal(sig)) {
          err = 0;
          break;
        }
      }
    }
  }

  return err;
}
