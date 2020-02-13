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

extern "C" void swtch(struct thread_context **, struct thread_context *);

static bool s_enabled = true;

// every mlfq entry has a this structure
// The scheduler is defined simply in OSTEP.
//   1. if Priority(A) > Priority(B), A runs
//   2. if Priority(A) == Priority(B), A and B run in RR
//   3. when a job enters the system, it has a high priority to maximize
//      responsiveness.
//   4a. If a job uses up an entire time slice while running, its priority is
//       reduced, (only moves down one queue)
//   4b. If a job gives up the CPU before
//       the timeslice is over, it stays at the same priority level.
//
struct mlfq_entry {
  // a simple round robin queue of tasks
  struct thread *task_queue;
  // so we can add to the end of the queue
  struct thread *last_task;
  int priority;

  long ntasks = 0;
  long timeslice = 0;

  spinlock queue_lock;
};

static struct mlfq_entry mlfq[SCHED_MLFQ_DEPTH];
static spinlock mlfq_lock = spinlock("mlfq_lock");

bool sched::init(void) {
  // initialize the mlfq
  for (int i = 0; i < SCHED_MLFQ_DEPTH; i++) {
    auto &Q = mlfq[i];
    Q.task_queue = NULL;
    Q.last_task = NULL;
    Q.priority = i;
    Q.ntasks = 0;
    Q.timeslice = 2;
  }

  return true;
}

static struct thread *get_next_thread(void) {
  // the queue is a singly linked list, so we need to
  // keep track of the last task in the queue so we can
  // remove the one we want to run from it
  struct thread *nt = nullptr;

  for (int i = SCHED_MLFQ_DEPTH - 1; i >= 0; i--) {
    auto &Q = mlfq[i];

    Q.queue_lock.lock();

    for (auto *t = Q.task_queue; t != NULL; t = t->sched.next) {
      if (t->state == PS_RUNNABLE) {
        nt = t;

        // remove from the queue
        if (t->sched.prev != NULL) t->sched.prev->sched.next = t->sched.next;
        if (t->sched.next != NULL) t->sched.next->sched.prev = t->sched.prev;
        if (Q.task_queue == t) Q.task_queue = t->sched.next;
        if (Q.last_task == t) Q.last_task = t->sched.prev;

        Q.ntasks--;
        t->sched.prev = NULL;
        t->sched.next = NULL;

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
int sched::add_task(struct thread *tsk) {
  // clamp the priority to the two bounds
  if (tsk->sched.priority > PRIORITY_HIGH) {
    tsk->sched.priority = PRIORITY_HIGH;
  }
  if (tsk->sched.priority < PRIORITY_IDLE) {
    tsk->sched.priority = PRIORITY_IDLE;
  }

  auto &Q = mlfq[tsk->sched.priority];

  // the task inherits the timeslice from the queue
  tsk->sched.timeslice = Q.timeslice;

  /* This critical section must be both locked and behind a cli()
   * because processes share this function with the scheduler. This means that
   * if a process were to be prempted within this section, the system would
   * deadlock as the scheduler would also try to grab the Q.queue_lock as well.
   * Because the scheduler cannot contend locks with threads, this is obviously
   * bad. To avoid this, we just make it so the thread cannot be interrupted in
   * this spot. (The only thing a thread could contend with is another CPU
   * core's scheduler, which is safe as the critical section is enormously
   * simple.
   */
  cpu::pushcli();
  // only lock this queue.
  Q.queue_lock.lock();

  if (Q.task_queue == nullptr) {
    // this is the only thing in the queue
    Q.task_queue = tsk;
    Q.last_task = tsk;

    tsk->sched.next = NULL;
    tsk->sched.prev = NULL;
  } else {
    // insert at the end of the list
    Q.last_task->sched.next = tsk;
    tsk->sched.next = NULL;
    tsk->sched.prev = Q.last_task;
    // the new task is the end
    Q.last_task = tsk;
  }

  Q.ntasks++;

  Q.queue_lock.unlock();
  cpu::popcli();

  return 0;
}

static void switch_into(struct thread &thd) {
  thd.locks.run.lock();
  cpu::current().current_thread = &thd;
  thd.state = PS_UNRUNNABLE;

  if (!thd.fpu.initialized) {
    asm volatile("fninit");
    asm volatile("fxsave64 (%0);" ::"r"(thd.fpu.state));
    thd.fpu.initialized = true;
  } else {
    asm volatile("fxrstor64 (%0);" ::"r"(thd.fpu.state));
  }

  thd.stats.run_count++;

  thd.sched.start_tick = cpu::get_ticks();

  cpu::switch_vm(&thd);

  thd.stats.last_cpu = thd.stats.current_cpu;
  thd.stats.current_cpu = smp::cpunum();

  swtch(&cpu::current().sched_ctx, thd.kern_context);

  // save the FPU state after the context switch returns here
  asm volatile("fxsave64 (%0);" ::"r"(thd.fpu.state));
  cpu::current().current_thread = nullptr;

  thd.locks.run.unlock();
}

void sched::do_yield(int st) {
  cpu::pushcli();

  auto &thd = *curthd;

  // thd.sched.priority = PRIORITY_HIGH;
  if (cpu::get_ticks() - thd.sched.start_tick >= thd.sched.timeslice) {
    // uh oh, we used up the timeslice, drop the priority!
    thd.sched.priority--;
  }

  thd.sched.priority = PRIORITY_HIGH;

  thd.state = st;
  thd.stats.last_cpu = thd.stats.current_cpu;
  thd.stats.current_cpu = -1;
  swtch(&thd.kern_context, cpu::current().sched_ctx);
  cpu::popcli();
}

// helpful functions wrapping different resulting task states
void sched::block() { sched::do_yield(PS_BLOCKED); }

void sched::yield() {
  // when you yield, you give up the CPU by ''using the rest of your
  // timeslice''
  // TODO: do this another way
  // auto tsk = cpu::task().get();
  // tsk->start_tick -= tsk->timeslice;
  do_yield(PS_RUNNABLE);
}
void sched::exit() { do_yield(PS_ZOMBIE); }

static void schedule_one() {
  auto thd = get_next_thread();

  if (thd == nullptr) {
    // idle loop when there isn't a task
    printk("nothing.\n");
    return;
  }

  cpu::pushcli();
  s_enabled = true;
  cpu::current().intena = 1;

  switch_into(*thd);

  cpu::popcli();

  /* add the task back to the mlfq if needed. */
  if (!thd->should_die) sched::add_task(thd);
}

void sched::run() {
  // re-calculated later using ''math''
  int boost_interval = 100;
  u64 last_boost = 0;

  for (;;) {
    schedule_one();

    auto ticks = cpu::get_ticks();

    // every S ticks or so, boost the processes at the bottom of the queue
    // into the top
    if (ticks - last_boost > boost_interval) {
      last_boost = ticks;
      int nmoved = 0;

      auto &HI = mlfq[PRIORITY_HIGH];

      HI.queue_lock.lock();
      for (int i = 0; i < PRIORITY_HIGH; i++) {
        auto &Q = mlfq[i];

        Q.queue_lock.lock();

        auto loq = Q.task_queue;

        if (loq != NULL) {
          for (auto *c = loq; c != NULL; c = c->sched.next) {
            if (!c->kern_idle) c->sched.priority = PRIORITY_HIGH;
          }

          // take the entire queue and add it to the end of the HIGH queue
          if (HI.task_queue != NULL) {
            assert(HI.last_task != NULL);
            HI.last_task->sched.next = loq;
            loq->sched.prev = HI.last_task;

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
  // send the EOI signal to the lapic
  irq::eoi(32 /* IRQ_TICK */);

  if (ticks >= beep_timeout) pcspeaker::clear();
  if (!enabled() || !cpu::in_thread()) return;

  // grab the current thread
  auto thd = cpu::thread();
  thd->sched.ticks++;
  // yield?
  if (ticks - thd->sched.start_tick >= thd->sched.timeslice) {
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
  auto waiter = curthd;
  waiter->wq.waiting_on = on;
  waiter->wq.flags = flags;

  waiter->wq.next = NULL;
  waiter->wq.prev = NULL;

  if (back == NULL) {
    assert(front == NULL);
    back = front = waiter;
  } else {
    back->wq.next = waiter;
    waiter->wq.prev = back;
    back = waiter;
  }

  lock.unlock();
  sched::do_yield(PS_BLOCKED);

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
    front = waiter->wq.next;
    // *nicely* awaken the thread
    waiter->awaken(false);
  }
}

bool waitqueue::should_notify(u32 val) {
  lock.lock();
  if (front != NULL) {
    if (front->wq.waiting_on <= val) {
      lock.unlock();
      return true;
    }
  }
  lock.unlock();
  return false;
}

void sched::before_iret(void) {
  if (!cpu::in_thread()) return;
  // exit via the scheduler if the task should die.
  if (curthd->should_die) sched::exit();
}
