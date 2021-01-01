#include <asm.h>
#include <cpu.h>
#include <lock.h>
#include <map.h>
#include <sched.h>
#include <single_list.h>
#include <sleep.h>
#include <syscall.h>
#include <time.h>
#include <wait.h>

#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

// #define SCHED_DEBUG
//

#define TIMESLICE_MIN 4

#ifdef SCHED_DEBUG
#define INFO(fmt, args...) printk("[SCHED] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

extern "C" void context_switch(struct thread_context **, struct thread_context *);


static bool s_enabled = true;
static sched::impl s_scheduler;

bool sched::init(void) { return true; }

static struct thread *get_next_thread(void) { return s_scheduler.pick_next(); }


static auto pick_next_thread(void) {
  if (curthd != NULL) {
    // printk_nolock("pick next thread from %d\n", curthd->tid);
  }
  if (cpu::current().next_thread == NULL) {
    cpu::current().next_thread = get_next_thread();
  }

  if (cpu::current().next_thread != NULL) {
    // printk_nolock("next %d\n", cpu::current().next_thread->tid);
  }
  return cpu::current().next_thread;
}


int sched::add_task(struct thread *tsk) {
	int res =  s_scheduler.add_task(tsk);
	return res;
}

int sched::remove_task(struct thread *t) { return s_scheduler.remove_task(t); }

static void switch_into(struct thread &thd) {
  thd.locks.run.lock();
  cpu::current().current_thread = &thd;
  thd.state = PS_RUNNING;
  arch_restore_fpu(thd);

  // update the statistics of the thread
  thd.stats.run_count++;
  thd.sched.start_tick = cpu::get_ticks();
  thd.stats.current_cpu = cpu::current().cpunum;

  // load up the thread's address space
  cpu::switch_vm(&thd);


  thd.stats.last_start_cycle = arch_read_timestamp();

  // Switch into the thread!
  context_switch(&cpu::current().sched_ctx, thd.kern_context);

  arch_save_fpu(thd);

  // Update the stats afterwards
  cpu::current().current_thread = nullptr;
  thd.stats.last_cpu = thd.stats.current_cpu;
  thd.stats.current_cpu = -1;

  thd.locks.run.unlock();
}


static spinlock blocked_threads_lock;
static list_head blocked_threads;


void sched::yield() {
  auto &thd = *curthd;

  // if the old thread is now dead, notify joiners
  if (thd.state == PS_ZOMBIE) {
    thd.joiners.wake_up();
  }

  arch_disable_ints();
  thd.stats.cycles += arch_read_timestamp() - thd.stats.last_start_cycle;
  thd.stats.last_cpu = thd.stats.current_cpu;
  thd.stats.current_cpu = -1;
  context_switch(&thd.kern_context, cpu::current().sched_ctx);
  arch_enable_ints();
}


void sched::set_state(int state) {
  auto &thd = *curthd;
  thd.state = state;
  // memory barrier!
  asm volatile("" : : : "memory");
}

void sched::do_yield(int st) {
  sched::set_state(st);
  sched::yield();
}



// helpful functions wrapping different resulting task states
void sched::block() { sched::do_yield(PS_INTERRUPTIBLE); }

/* Unblock a thread */
void sched::unblock(thread &thd, bool interrupt) {
  if (thd.state != PS_INTERRUPTIBLE) {
    /* Hmm, not sure what to do here. */
    // printk(KERN_WARN "Attempt to wake up thread %d which is not PS_INTERRUPTIBLE\n", thd.tid);
  }
  thd.wq.rudely_awoken = interrupt;
  thd.state = PS_RUNNING;

  /*
blocked_threads_lock.lock();
// assert(!thd.blocked_threads.is_empty());
  thd.blocked_threads.del_init();
blocked_threads_lock.unlock();
  */
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
    /*
     * The loop here is simple. Wait for an interrupt, handle it (implicitly) then
     * yield back to the scheduler if there is a task ready to run.
     */
    arch_halt();
    // Check for a new thread to run, and if there is one, yield so we can change to it.
    if (pick_next_thread() != NULL) {
      sched::yield();
    }
  }
}


void sched::run() {
  // per-scheduler idle threads do not exist in the scheduler queue.
  auto *idle_thread = sched::proc::spawn_kthread("idle task", idle_task, NULL);
  // the idle thread should not be preemptable
  idle_thread->preemptable = false;

  cpu::current().in_sched = true;
  while (1) {
    struct thread *thd = pick_next_thread();
    cpu::current().next_thread = NULL;

    if (thd == nullptr) {
      // idle loop when there isn't a task
      cpu::current().kstat.iticks++;
      switch_into(*idle_thread);
      continue;
    }

    s_enabled = true;

    switch_into(*thd);

    sched::add_task(thd);
    // boost();
  }
  panic("scheduler should not have gotten back here\n");
}

bool sched::enabled() { return s_enabled; }

void sched::handle_tick(u64 ticks) {
  if (!enabled() || !cpu::in_thread()) return;

  // grab the current thread
  auto thd = cpu::thread();

  /* We don't get preempted if we aren't currently runnable. See wait.cpp for why */
  if (thd->state != PS_RUNNING) return;

  if (thd->preemptable == false) return;

  if (thd->proc.ring == RING_KERN) {
    cpu::current().kstat.kticks++;
  } else {
    cpu::current().kstat.uticks++;
  }
  thd->sched.ticks++;

  auto has_run = ticks - thd->sched.start_tick;

  // yield?
  if (has_run >= thd->sched.timeslice) {
#if CONFIG_PREFETCH_NEXT_THREAD
    // pick a thread to go into next. If there is nothing to run,
    // don't yield to the scheduler. This improves some stuff's latencies
    if (pick_next_thread() != NULL) {
      sched::yield();
    }
#else
    sched::yield();
#endif
  }
}




#define SIGACT_IGNO 0
#define SIGACT_TERM 1
#define SIGACT_STOP 2
#define SIGACT_CONT 3


static int default_signal_action(int signal) {
  ASSERT(signal && signal < 64);

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

void sched::dispatch_signal(int sig) {
  // sanity check
  if (sig < 0 || sig > 63) return;

  auto &action = curproc->sig.handlers[sig];

  if (sig == SIGSTOP) {
    printk("TODO: SIGSTOP\n");
    return;
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
        return;
      case SIGACT_TERM:
        sys::exit_proc(128 + sig);
        return;
      case SIGACT_IGNO:
        return;
      case SIGACT_CONT:
        printk("TODO: SIGACT_CONT!\n");
        return;
    }
  }

  // whatver the arch needs to do
  arch_dispatch_function((void *)action.sa_handler, sig);
}


void sched::before_iret(bool userspace) {
  if (!cpu::in_thread()) return;
  // exit via the scheduler if the task should die.
  if (curthd->should_die) sched::exit();

  long sig_to_handle = -1;


  while (1) {
    sig_to_handle = -1;
    if (curthd->sig.pending != 0) {
      for (int i = 0; i < 63; i++) {
        if ((curthd->sig.pending & SIGBIT(i)) != 0) {
          if (true || (curthd->sig.mask & SIGBIT(i))) {
            // Mark this signal as handled.
            curthd->sig.pending &= ~SIGBIT(i);
            sig_to_handle = i;
            break;
          }
        }
      }
    }

    if (sig_to_handle != -1) {
      sched::dispatch_signal(sig_to_handle);
    } else {
      break;
    }
  }
}


int sys::usleep(unsigned long n) {
  /* If the microseconds is less than one millisecond, busy loop... lol */
  if (n < 1000 * 1) {
    unsigned long end = time::now_us() + n;
    while (1) {
      if (time::now_us() >= end) break;
      arch_relax();
    }
    return 0;
  }
  return do_usleep(n);
}
