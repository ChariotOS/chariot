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

static bool s_enabled = true;

// every mlfq entry has a this structure
// The scheduler is defined simply in OSTEP.
//   1. if Priority(A) > Priority(B), A runs
//   2. if Priority(A) == Priority(B), A and B run in RR
//   3. when a job enters the system, it has a high priority to maximize
//      responsiveness.
//   4a. If a job uses up an entire time slioce while running, its priority is
//       reduced, (only moves down one queue)
//   4b. If a job gives up the CPU before
//       the timeslice is over, it stays at the same priority level.
//
struct mlfq_entry {
  // a simple round robin queue of tasks
  struct task *task_queue;
  // so we can add to the end of the queue
  struct task *last_task;
  int priority;

  mutex_lock queue_lock;
};

static mutex_lock mlfq_lock("sched::mlfq");
// 11 arbitrary entries
static struct mlfq_entry mlfq[PRIORITY_HIGH + 1 /* for IDLE at 0 */];

static struct task *next_task(void) {
  // the queue is a singly linked list, so we need to
  // keep track of the last task in the queue so we can
  // remove the one we want to run from it
  struct task *nt = nullptr;

  for (int i = PRIORITY_HIGH; i >= 0; i--) {
    auto &Q = mlfq[i];

    Q.queue_lock.lock();


    for (auto *t = Q.task_queue; t != NULL; t = t->next) {
      if (t->state == PS_RUNNABLE) {
        nt = t;
        // remove from the queue
        if (t->prev != NULL)
          t->prev->next = t->next;

        if (t->next != NULL)
          t->next->prev = t->prev;

        if (Q.task_queue == t) Q.task_queue = t->next;
        if (Q.last_task == t) Q.last_task = t->prev;

        t->prev = NULL;
        t->next = NULL;

        break;
      }
    }

    Q.queue_lock.unlock();

    if (nt != nullptr) {
      break;
    }
  }

  return nt;
}

bool sched::init(void) {
  // initialize the mlfq
  for (int i = 0; i < PRIORITY_HIGH; i++) {
    mlfq[i].task_queue = NULL;
    mlfq[i].last_task = NULL;
    mlfq[i].priority = i;
  }
  return true;
}

// add a task to a mlfq entry based on tsk->priority
int sched::add_task(struct task *tsk) {
  // clamp the priority to the two bounds
  if (tsk->priority > PRIORITY_HIGH) tsk->priority = PRIORITY_HIGH;
  if (tsk->priority < PRIORITY_IDLE) tsk->priority = 0;

  auto &Q = mlfq[tsk->priority];

  // only lock this queue.
  Q.queue_lock.lock();

  if (Q.task_queue == nullptr) {
    // this is the only thing in the queue
    Q.task_queue = tsk;
    Q.last_task = tsk;

    tsk->next = NULL;
    tsk->prev = NULL;
  } else {

    // insert at the end of the list
    Q.last_task->next = tsk;
    tsk->next = NULL;
    tsk->prev = Q.last_task;
    // the new task is the end
    Q.last_task = tsk;
  }
  Q.queue_lock.unlock();
  return 0;
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

  auto tsk = cpu::task().get();

  if (cpu::get_ticks() - tsk->start_tick >= tsk->timeslice) {
    // uh oh, we used up the timeslice, drop the priority!
    if (!tsk->is_idle_thread) tsk->priority--;
  } else {
    if (!tsk->is_idle_thread) tsk->priority++;
  }

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
    halt();
    // idle loop when there isn't a task
    return;
  }

  cpu::pushcli();

  s_enabled = true;
  cpu::current().intena = 1;
  switch_into(tsk);

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
  if (ticks >= beep_timeout) pcspeaker::clear();

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
