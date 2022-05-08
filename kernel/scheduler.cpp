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


#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

volatile long monitor_tid = 0;
volatile bool did_panic = false;  // printk.h
static bool s_enabled = true;



int rt::Task::make_runnable(int cpu, bool admit) {
  cpu::Core *target_core = nullptr;


  // cpu of -1 means "self"
  if (cpu == -1) {
    target_core = &core();
  } else {
    target_core = cpu::get(cpu);
    if (target_core == nullptr) {
      printf(KERN_WARN "Could not find cpu %d during rt::make_runnable. Defaulting to self\n", cpu);
      target_core = &core();
    }
  }

  if (target_core == nullptr) {
    return -ENOENT;
  }


  auto &s = target_core->local_scheduler;
  // grab a scoped lock
  auto lock = s.lock();

  if (admit) {
    if (s.admit(this, time::now_us())) {
      printf(KERN_WARN "Failed to admit thread\n");
      return -1;
    } else {
      // DEBUG("Admitted thread %p (tid=%d)\n", thread, task->tid);
    }
  }

  if (constraint().type == APERIODIC) {
    s.aperiodic.enqueue(this);

    this->rt_status = ADMITTED;
  } else {
    printf(KERN_WARN "Cannot handle non-aperiodic tasks yet\n");
    return -ENOTIMPL;
  }

  return 0;
}


rt::Task::Task(Constraints c) : m_constraint(c) {
  // printf("Create Task\n");
}
rt::Task::~Task(void) {
  // printf("Destroy task\n");
}

void rt::Task::reset_state(void) {
  start_time = 0;
  cur_run_time = 0;
  run_time = 0;
  deadline = 0;
  exit_time = 0;
}

void rt::Task::reset_stats(void) {
  if (m_constraint.type == APERIODIC) {
    arrival_count = 1;
  } else {
    arrival_count = 0;
  }

  resched_count = 0;
  resched_long_count = 0;
  switch_in_count = 0;
  miss_count = 0;
  miss_time_sum = 0;
  miss_time_sum2 = 0;
}

rt::Scheduler::Scheduler(cpu::Core &core) : m_core(core) {}


bool rt::Scheduler::admit(rt::Task *task, uint64_t now) {
  const auto &constraint = task->constraint();



  if (constraint.type == ConstraintType::APERIODIC) {
    // Always admit aperiodic tasks
    task->reset_state();
    task->reset_stats();

    task->deadline = constraint.aperiodic.priority;

    return true;
  }

  // runnable.enqueue(task);
  return false;
}

rt::Task *rt::Scheduler::reschedule(void) { return runnable.dequeue(); }


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


void rt::PriorityQueue::enqueue(rt::Task *task) {
  assert(task->queue_type == NO_QUEUE);
  task->queue_type = this->type;
  m_size++;
  // place them in the queue
  rb_insert(m_root, &task->prio_node, [&](struct rb_node *o) {
    auto *other = rb_entry(o, rt::Task, prio_node);
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

rt::Task *rt::PriorityQueue::peek(void) {
  // save some time
  if (m_size == 0) return nullptr;
  // get the first in the tree (which is sorted by prio/deadline)
  auto *first_node = rb_first(&m_root);
  if (first_node == nullptr) return nullptr;
  // grab the task and return it
  rt::Task *task = rb_entry(first_node, rt::Task, prio_node);
  assert(task->queue_type == this->type);
  return task;
}


void rt::PriorityQueue::remove(rt::Task *task) {
  if (task == nullptr) return;
  // This task must be in this queue (we assume there is one type of queue per
  // rt::Scheduler)
  assert(task->queue_type == this->type);
  rb_erase(&task->prio_node, &m_root);
  m_size--;
  task->queue_type = NO_QUEUE;
}

rt::Task *rt::PriorityQueue::dequeue(void) {
  // save some time
  if (m_size == 0) return nullptr;
  // peek at the next node, remove it, then return it
  rt::Task *task = peek();
  if (task == nullptr) return nullptr;
  remove(task);
  return task;
}



void rt::Queue::enqueue(rt::Task *task) {
  m_size++;
  task->queue_type = this->type;
  m_list.add_tail(&task->queue_node);
}

rt::Task *rt::Queue::dequeue(void) {
  auto next = m_list.next;
  if (next == &m_list) return nullptr;

  next->del_init();
  m_size--;
  auto *task = list_entry(next, rt::Task, queue_node);
  task->queue_type = NO_QUEUE;
  return task;
}


// copied from
#define MLFQ_NQUEUES 4
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
      barrier();
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
      barrier();

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
      // if the thread is not still running (blocked on I/O), improve their priority
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
      printf_nolock("\e[2J");

      for (int prio = 0; prio < MLFQ_NQUEUES; prio++) {
        auto &q = queues[prio];
        printf_nolock("prio %02d:", prio);
        for (struct Thread *thd = q.front; thd != NULL; thd = thd->next) {
          printf_nolock(" '%s(good: %d)'", thd->proc.name.get(), thd->sched.good_streak);
        }
        printf_nolock("\n");
      }
      printf_nolock("\n");
    }
  }

  ck::ref<Thread> get_next(void) {
    scoped_irqlock l(lock);
    for (int prio = 0; prio < MLFQ_NQUEUES; prio++) {
      if (ck::ref<Thread> cur = queues[prio].pick_next(); cur != nullptr) return cur;
    }

    return nullptr;
  }



  // steal to another cpu, `thief`
  ck::ref<Thread> steal(int thief) {
    bool locked = false;
    auto f = lock.try_lock_irqsave(locked);
    // printf_nolock("%d tries to steal from %d\n", thief, core);

    if (locked) {
      for (int prio = 0; prio < MLFQ_NQUEUES; prio++) {
        // TODO: make sure it can be taken by the thief
        if (ck::ref<Thread> cur = queues[prio].pick_next(); cur != nullptr) {
          lock.unlock_irqrestore(f);
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
    barrier();
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
};


// each cpu has their own MLFQ. If a core does not have any threads in it's queue, it can steal
// runnable threads from other queues.
static struct mlfq queues[CONFIG_MAX_CPUS];


void sched::dump(void) {
  printf("------ Scheduler Dump ------\n");
  for (int i = 0; i < CONFIG_MAX_CPUS; i++) {
    if (queues[i].active == false) continue;

    auto &q = queues[i];
    auto *core = cpu::get(q.core);
    auto &current = core->current_thread;
    printf("Core %d. current=%d\n", q.core, current ? current->tid : -1);
    for (int d = 0; d < MLFQ_NQUEUES; d++) {
      auto &m = q.queues[d];
      printf("  - %2d: ", d);
      for (auto cur = m.front; cur != nullptr; cur = cur->next) {
        printf(" %4d", cur->tid);
      }
      printf("\n");
    }
  }
}




#ifdef SCHED_DEBUG
#define INFO(fmt, args...) printf("[SCHED] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

extern "C" void context_switch(struct thread_context **, struct thread_context *);



static auto &my_queue(void) { return queues[cpu::current().id]; }


bool sched::init(void) {
  auto &q = my_queue();
  q.core = cpu::current().id;
  q.active = true;

  return true;
}



// work-steal from other cores if they have runnable threads
static ck::ref<Thread> worksteal(void) {
  // Get the number of processors
  int nproc = cpu::nproc();
  // divide by zero and infinite loop safety
  if (nproc <= 1) return nullptr;

  auto &q = my_queue();
  unsigned int target = q.core;

  do {
    target = (q.next_worksteal++) % nproc;
  } while (target == q.core);

  if (q.next_worksteal >= nproc) q.next_worksteal = 0;
  if (!queues[target].active) return nullptr;

  ck::ref<Thread> t = queues[target].steal(q.core);



  // printf_nolock("cpu %d stealing from cpu %d : %d\n", q.core, target, t);

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

static void switch_into(ck::ref<Thread> thd) { thd->run(); }



sched::yieldres sched::yield() {
  sched::yieldres r = sched::yieldres::None;
  auto &thd = *curthd;

  arch_disable_ints();
  barrier();


  thd.stats.cycles += arch_read_timestamp() - thd.stats.last_start_cycle;
  thd.stats.last_cpu = thd.stats.current_cpu;
  thd.stats.current_cpu = -1;
  context_switch(&thd.kern_context, cpu::current().sched_ctx);

  if (thd.wq.rudely_awoken) r = sched::yieldres::Interrupt;

  barrier();
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
  thd.wq.rudely_awoken = interrupt;
  thd.set_state(PS_RUNNING);
  __sync_synchronize();
}

void sched::exit() {
  curthd->exit();
  yield();
}

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
  if (false && active_cores == 0) {
    for (int c = 0; c < 1000; c++) {
      auto &s = core().local_scheduler;
      for (int i = 0; i < c; i++) {
        auto now = arch_read_timestamp();
        auto t = new rt::Task();
        t->deadline = now + arch_ns_to_timestamp(3000 * 1000);  // 3us

        s.admit(t, now);
      }

      auto start = arch_read_timestamp();

      while (true) {
        auto t = s.reschedule();
        if (t == NULL) break;
        delete t;
        // printf("deadline: %lu\n", t->deadline);
      }
      auto end = arch_read_timestamp();
      printf("%d, %llu\n", c, end - start);
    }
    sys::shutdown();
  }

  __atomic_fetch_add(&active_cores, 1, __ATOMIC_SEQ_CST);
  arch_disable_ints();
  // per-scheduler idle threads do not exist in the scheduler queue.
  unsigned long has_run = 0;
  ck::ref<Thread> idle_thread = sched::proc::spawn_kthread("idle task", idle_task, NULL);
  idle_thread->preemptable = true;
  core().in_sched = true;




  while (1) {
    if (has_run++ >= 100) {
      has_run = 0;
      my_queue().boost();
    }

    ck::ref<Thread> thd = pick_next_thread();
    cpu::current().next_thread = nullptr;


    if (did_panic && thd != nullptr) {
      // only run kernel threads, and the monitor thread.
      if (thd->tid != monitor_tid && thd->proc.ring == RING_USER) {
        sched::add_task(thd);
        thd = nullptr;
      }
    }

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

    // printf_nolock("%s %d %d\n", thd->proc.name.get(), thd->tid, thd->ref_count());
    auto start = cpu::get_ticks();
    switch_into(thd);
    auto end = cpu::get_ticks();
    auto ran = end - start;

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
      scoped_irqlock l(thd->joinlock);
      thd->set_state(PS_ZOMBIE);
      thd->joiners.wake_up_all();
    }
    if (thd->get_state() != PS_ZOMBIE && !thd->should_die) {
      // add the task back to the scheduler
      sched::add_task(thd);
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
  if (thd->should_die /* && thd->irq_depth == 1 */) {
    c.woke_someone_up = false;
    sched::yield();
  }

  if (time::stabilized()) thd->last_start_utime_us = time::now_us();

  // if its not running, it is setting up a waitqueue or exiting, so we don't
  // want to screw that up by prematurely yielding.
  if (thd->state != PS_RUNNING) {
    return;
  }

  bool out_of_time = thd->sched.has_run >= thd->sched.timeslice;
  if (out_of_time || cpu::current().next_thread != nullptr || c.woke_someone_up) {
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
