#include <asm.h>
#include <cpu.h>
#include <lock.h>
#include <map.h>
#include <sched.h>
#include <single_list.h>
#include <syscall.h>
#include <time.h>
#include <wait.h>
#include "../../arch/x86/fpu.h"


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

extern "C" void swtch(struct thread_context **, struct thread_context *);


static bool s_enabled = true;
static sched::impl s_scheduler;

bool sched::init(void) { return true; }

static struct thread *get_next_thread(void) { return s_scheduler.pick_next(); }


static auto pick_next_thread(void) {
  if (cpu::current().next_thread == NULL) {
    cpu::current().next_thread = get_next_thread();
  }
  return cpu::current().next_thread;
}


// add a task to a mlfq entry based on tsk->priority
int sched::add_task(struct thread *tsk) { return s_scheduler.add_task(tsk); }

int sched::remove_task(struct thread *t) { return s_scheduler.remove_task(t); }

static void switch_into(struct thread &thd) {
  thd.locks.run.lock();
  cpu::current().current_thread = &thd;
  thd.state = PS_RUNNING;
  arch::restore_fpu(thd);

  // update the statistics of the thread
  thd.stats.run_count++;
  thd.sched.start_tick = cpu::get_ticks();
  thd.stats.current_cpu = cpu::current().cpunum;

  // load up the thread's address space
  cpu::switch_vm(&thd);

  thd.stats.last_start_cycle = arch::read_timestamp();

  // Switch into the thread!
  swtch(&cpu::current().sched_ctx, thd.kern_context);

  arch::save_fpu(thd);

  // Update the stats afterwards
  cpu::current().current_thread = nullptr;
  thd.stats.last_cpu = thd.stats.current_cpu;
  thd.stats.current_cpu = -1;

  thd.locks.run.unlock();

}

void sched::do_yield(int st) {

  auto &thd = *curthd;

	// if the old thread is now dead, notify joiners
	if (st == PS_ZOMBIE) {
		thd.joiners.wake_up();
	}

  arch::cli();

  thd.stats.cycles += arch::read_timestamp() - thd.stats.last_start_cycle;

  thd.state = st;
  thd.stats.last_cpu = thd.stats.current_cpu;
  thd.stats.current_cpu = -1;
  swtch(&thd.kern_context, cpu::current().sched_ctx);

  arch::sti();
}

// helpful functions wrapping different resulting task states
void sched::block() { sched::do_yield(PS_INTERRUPTIBLE); }

void sched::yield() {
  // when you yield, you give up the CPU by ''using the rest of your
  // timeslice''
  // TODO: do this another way
  // auto tsk = cpu::task().get();
  // tsk->start_tick -= tsk->timeslice;
  do_yield(PS_RUNNING);
}
void sched::exit() {
	do_yield(PS_ZOMBIE);
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
    /*
     * The loop here is simple. Wait for an interrupt, handle it (implicitly) then
     * yield back to the scheduler if there is a task ready to run.
     */
    arch::halt();
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
  arch::dispatch_function((void *)action.sa_handler, sig);
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

sleep_blocker::sleep_blocker(unsigned long us_to_sleep) {
  auto now = time::now_us();
  end_us = now + us_to_sleep;
  // printk("waiting %zuus from %zu till %zu\n", us_to_sleep, now, end_us);
}

bool sleep_blocker::should_unblock(struct thread &t, unsigned long now_us) {
  bool ready = now_us >= end_us;

  if (ready) {
    // printk("accuracy: %zdus\n", (long)now_us - (long)end_us);
  } else {
    // printk("time to go %zuus\n", end_us - now_us);
  }

  return ready;
}

int sys::usleep(unsigned long n) {
  if (1) {
    unsigned long end = time::now_us() + n;
    while (1) {
      if (time::now_us() >= end) break;
      arch::relax();
    }
    return 0;
  }
  return 0;
}
