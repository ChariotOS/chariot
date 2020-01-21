#include <asm.h>
#include <cpu.h>
#include <lock.h>
#include <map.h>
#include <pcspeaker.h>
#include <sched.h>
#include <single_list.h>
#include <smp.h>
#include <wait.h>

// #define SCHED_DEBUG
//

#define TIMESLICE_MIN 4

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

  long ntasks = 0;
  long timeslice = 0;

  spinlock queue_lock;
};

static struct mlfq_entry mlfq[SCHED_MLFQ_DEPTH];
static spinlock mlfq_lock;

bool sched::init(void) {
  // initialize the mlfq
  for (int i = 0; i < SCHED_MLFQ_DEPTH; i++) {
    auto &Q = mlfq[i];

    Q.task_queue = NULL;
    Q.last_task = NULL;
    Q.priority = i;
    Q.ntasks = 0;

    Q.timeslice = 1;
  }

  return true;
}

static struct task *next_task(void) {
  // the queue is a singly linked list, so we need to
  // keep track of the last task in the queue so we can
  // remove the one we want to run from it
  struct task *nt = nullptr;

  for (int i = SCHED_MLFQ_DEPTH - 1; i >= 0; i--) {
    auto &Q = mlfq[i];

    Q.queue_lock.lock();

    for (auto *t = Q.task_queue; t != NULL; t = t->next) {
      if (t->state == PS_RUNNABLE) {
        nt = t;
        // remove from the queue
        if (t->prev != NULL) t->prev->next = t->next;
        if (t->next != NULL) t->next->prev = t->prev;
        if (Q.task_queue == t) Q.task_queue = t->next;
        if (Q.last_task == t) Q.last_task = t->prev;

        Q.ntasks--;
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

// add a task to a mlfq entry based on tsk->priority
int sched::add_task(struct task *tsk) {
  // clamp the priority to the two bounds
  if (tsk->priority > PRIORITY_HIGH) tsk->priority = PRIORITY_HIGH;
  if (tsk->priority < PRIORITY_IDLE) tsk->priority = PRIORITY_IDLE;

  auto &Q = mlfq[tsk->priority];

  // the task inherits the timeslice from the queue
  tsk->timeslice = Q.timeslice;
  // only lock this queue.
  Q.queue_lock.lock();

  // we dont want to be interrupted here because this API can take the same lock
  // that the scheduler needs. This results in a deadlock randomly.
  if (cpu::in_thread()) cpu::pushcli();

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

  Q.ntasks++;

  Q.queue_lock.unlock();
  if (cpu::in_thread()) cpu::popcli();

  return 0;
}

static void switch_into(struct task *tsk) {
  tsk->run_lock.lock();
  cpu::current().current_thread = tsk;
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

  tsk->last_cpu = tsk->current_cpu;
  tsk->current_cpu = smp::cpunum();

  swtch(&cpu::current().sched_ctx, tsk->ctx);

  asm volatile("fxsave64 (%0);" ::"r"(tsk->fpu_state));
  cpu::current().current_thread = nullptr;

  tsk->run_lock.unlock();
}

static void schedule() {
  cpu::task()->current_cpu = -1;
  swtch(&cpu::task()->ctx, cpu::current().sched_ctx);
}

static void do_yield(int st) {
  cpu::pushcli();

  auto tsk = cpu::task().get();

  tsk->priority = PRIORITY_HIGH;
  if (cpu::get_ticks() - tsk->start_tick >= tsk->timeslice) {
    // uh oh, we used up the timeslice, drop the priority!
    if (!tsk->is_idle_thread && tsk->priority > 0) {
      tsk->priority--;
    }
  }

  cpu::task()->state = st;

  schedule();

  cpu::popcli();
}

// helpful functions wrapping different resulting task states
void sched::block() { do_yield(PS_BLOCKED); }
void sched::yield() {
  // when you yield, you give up the CPU by ''using the rest of your timeslice''
  // TODO: do this another way
  // auto tsk = cpu::task().get();
  // tsk->start_tick -= tsk->timeslice;
  do_yield(PS_RUNNABLE);
}
void sched::exit() { do_yield(PS_ZOMBIE); }

static void schedule_one() {
  auto tsk = next_task();

  if (tsk == nullptr) {
    // idle loop when there isn't a task
    return;
  }

  cpu::pushcli();
  s_enabled = true;
  cpu::current().intena = 1;
  switch_into(tsk);

  cpu::popcli();

  /* add the task back to the */
  if (!tsk->should_die)
    sched::add_task(tsk);
}

void sched::run() {
  // re-calculated later using ''math''
  int boost_interval = 100;
  u64 last_boost = 0;

  for (;;) {
    schedule_one();

    auto ticks = cpu::get_ticks();

    // every S ticks or so, boost the processes at the bottom of the queue into
    // the top
    if (ticks - last_boost > boost_interval) {
      last_boost = ticks;
      int nmoved = 0;

      auto &HI = mlfq[PRIORITY_HIGH];

      HI.queue_lock.lock();
      for (int i = 0; i < PRIORITY_HIGH /* skipping HIGH */; i++) {
        auto &Q = mlfq[i];

        Q.queue_lock.lock();

        auto loq = Q.task_queue;

        if (loq != NULL) {
          for (auto *c = loq; c != NULL; c = c->next) {
            if (!c->is_idle_thread) c->priority = PRIORITY_HIGH;
          }

          // take the entire queue and add it to the end of the HIGH queue
          if (HI.task_queue != NULL) {
            assert(HI.last_task != NULL);
            HI.last_task->next = loq;
            loq->prev = HI.last_task;

            // inherit the last task from the old Q
            HI.last_task = Q.last_task;
          } else {
            assert(HI.ntasks == 0);
            HI.task_queue = Q.task_queue;
            HI.last_task = Q.last_task;
          }

          HI.ntasks += Q.ntasks;
          nmoved += Q.ntasks;

          // zero out this queue
          Q.task_queue = Q.last_task = NULL;
          Q.ntasks = 0;
        }

        Q.queue_lock.unlock();
      }

      boost_interval = 500;
      // TODO: calculate a new boost interval here.

      HI.queue_lock.unlock();
    }
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

  if (!enabled() || !cpu::in_thread()) {
    return;
  }

  auto tsk = cpu::task();
  tsk->ticks++;
  // yield?
  if (ticks - tsk->start_tick >= tsk->timeslice) {
    if (cpu::preempt_disabled()) {
      printk("preempt_disabled\n");
      // return;
    }
    sched::yield();
  }
}

waitqueue::waitqueue(const char *name) : name(name) {}

int waitqueue::wait(u32 on) { return do_wait(on, 0); }

void waitqueue::wait_noint(u32 on) { do_wait(on, WAIT_NOINT); }

int waitqueue::do_wait(u32 on, int flags) {
  lock.lock();

  if (navail > 0) {
    navail--;
    lock.unlock();
    return 0;
  }

  assert(navail == 0);

  // add to the wait queue
  auto waiter = cpu::task().get();
  waiter->waiting_on = on;
  waiter->wait_flags = flags;

  waiter->wq_next = NULL;
  waiter->wq_prev = NULL;

  if (back == NULL) {
    assert(front == NULL);
    back = front = waiter;
  } else {
    back->wq_next = waiter;
    waiter->wq_prev = back;
    back = waiter;
  }

  lock.unlock();
  do_yield(PS_BLOCKED);

  // TODO: read form the thread if it was rudely notified or not
  return 0;
}

void waitqueue::notify() {
  scoped_lock lck(lock);

  if (front == NULL) {
    navail++;
  } else {
    auto waiter = front;
    if (front == back) back = NULL;
    front = waiter->wq_next;
    // *nicely* awaken the thread
    waiter->awaken(false);
  }
}

bool waitqueue::should_notify(u32 val) {
  scoped_lock lck(lock);
  if (front != NULL) {
    if (front->waiting_on <= val) {
      return true;
    }
  }
  return false;
}


void sched::before_iret(void) {

  if (!cpu::in_thread()) return;
  // exit via the scheduler if the task should die.
  if (cpu::task()->should_die)
    sched::exit();
}
