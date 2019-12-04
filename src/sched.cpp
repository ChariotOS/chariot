#include <asm.h>
#include <cpu.h>
#include <lock.h>
#include <map.h>
#include <pcspeaker.h>
#include <sched.h>
#include <single_list.h>
#include <wait.h>

// #define SCHED_DEBUG

#ifdef SCHED_DEBUG
#define INFO(fmt, args...) printk("[SCHED] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

extern "C" void swtch(struct task_context **, struct task_context *);

static mutex_lock process_lock("processes");
static bool s_enabled = true;

static task *task_queue;
static task *last_task;

static struct task *next_task(void) {
  struct task *nt = nullptr;
  process_lock.lock();

  if (task_queue != nullptr) {
    nt = task_queue;
    task_queue = nt->next;
  }
  process_lock.unlock();

  return nt;
}

int sched::add_task(struct task *tsk) {
  process_lock.lock();
  if (task_queue == nullptr) {
    task_queue = tsk;
    last_task = tsk;
  } else {
    last_task->next = tsk;
    last_task = tsk;
  }
  process_lock.unlock();
  return 0;
}

bool sched::init(void) {
  process_lock.lock();
  process_lock.unlock();
  return true;
}

static void switch_into(struct task *tsk) {
  cpu::current().current_thread = tsk;
  tsk->ticks++;
  tsk->start_tick = cpu::get_ticks();
  tsk->state = PS_UNRUNNABLE;

  if (!tsk->fpu_initialized) {
    asm volatile("fninit");
    asm volatile("fxsave64 (%0);" ::"r"(tsk->fpu_state));
    tsk->fpu_initialized = true;
  } else {
    asm volatile("fxrstor64 (%0);" ::"r"(tsk->fpu_state));
  }

  cpu::switch_vm(tsk);

  swtch(&cpu::current().sched_ctx, tsk->ctx);

  asm volatile("fxsave64 (%0);" ::"r"(tsk->fpu_state));
  cpu::current().current_thread = nullptr;
}

static void schedule() {

  auto old_ncli = cpu::ncli();


  if (cpu::ncli() > 1) {
    cpu::current().ncli = 1;
    // panic("schedule must have ncli == 1, instead ncli = %d", cpu::ncli());
  }

  int intena = cpu::current().intena;
  swtch(&cpu::task()->ctx, cpu::current().sched_ctx);

  cpu::current().ncli = old_ncli;

  cpu::current().intena = intena;
}

static void do_yield(int st) {
  cpu::pushcli();

  cpu::task()->state = st;
  schedule();

  cpu::popcli();
}

void sched::block() { do_yield(PS_BLOCKED); }

void sched::yield() { do_yield(PS_RUNNABLE); }

void sched::exit() { do_yield(PS_ZOMBIE); }

static void schedule_one() {
  auto tsk = next_task();

  if (tsk == nullptr) {
    // idle loop when there isn't a task
    halt();
    return;
  }

  cpu::pushcli();

  if (tsk->state == PS_RUNNABLE) {
    s_enabled = true;
    cpu::current().intena = 1;
    switch_into(tsk);
  } else {
    // printk("proc %d was not runnable\n", tsk->tid);
  }

  cpu::popcli();

  sched::add_task(tsk);
}

void sched::run() {
  for (;;) {
    schedule_one();
  }
  panic("scheduler should not have gotten back here\n");
}

bool sched::enabled() { return s_enabled; }

static u64 beep_timeout = 0;

void sched::play_tone(int frq, int dur) {
  pcspeaker::set(frq);
  beep_timeout = cpu::get_ticks() + dur;
}

void sched::beep(void) { play_tone(440, 25); }

void sched::handle_tick(u64 ticks) {
  if (ticks >= beep_timeout) {
    pcspeaker::clear();
  }

  if (!enabled() || !cpu::in_thread() || cpu::task()->state != PS_UNRUNNABLE) {
    return;
  }

  auto tsk = cpu::task();
  // yield?
  if (ticks - tsk->start_tick >= tsk->timeslice) {
    sched::yield();
  }
}

waitqueue::waitqueue(const char *name) : name(name), lock(name) {}

void waitqueue::wait(u32 on) {
  lock.lock();

  if (navail > 0) {
    navail--;
    lock.unlock();
    return;
  }

  assert(navail == 0);

  // add to the wait queue
  struct waitqueue_elem e;
  e.waiter = cpu::task();
  e.waiting_on = on;
  elems.append(e);
  lock.unlock();
  do_yield(PS_BLOCKED);
}

void waitqueue::notify() {
  scoped_lock lck(lock);
  if (elems.is_empty()) {
    navail++;
  } else {
    auto e = elems.take_first();

    assert(e.waiter->state == PS_BLOCKED);
    e.waiter->state = PS_RUNNABLE;
  }
}

bool waitqueue::should_notify(u32 val) {
  scoped_lock lck(lock);
  if (!elems.is_empty()) {
    if (elems.first().waiting_on <= val) {
      return true;
    }
  }
  return false;
}
