#include <asm.h>
#include <cpu.h>
#include <lock.h>
#include <pcspeaker.h>
#include <sched.h>
#include <single_list.h>
#include <task.h>

// #define SCHED_DEBUG

#ifdef SCHED_DEBUG
#define INFO(fmt, args...) printk("[SCHED] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

extern "C" void swtch(context_t **, context_t *);

static mutex_lock task_lock("task");
int next_pid = 1;
static bool s_enabled = true;

static task *task_queue;
static task *last_task;
// static single_list<task *> task_queue;

static task *talloc(const char *name, gid_t group, int ring) {
  task_lock.lock();
  auto t = new task(name, next_pid++, group, ring);
  task_lock.unlock();

  return t;
}

static task *next_task(void) {
  task *nt = nullptr;
  task_lock.lock();

  if (task_queue != nullptr) {
    nt = task_queue;
    task_queue = nt->next;
  }
  task_lock.unlock();

  return nt;
}

static void add_task(task *tsk) {
  task_lock.lock();

  if (task_queue == nullptr) {
    task_queue = tsk;
    last_task = tsk;
  } else {
    last_task->next = tsk;
    last_task = tsk;
  }
  // task_queue.append(tsk);
  task_lock.unlock();
}

bool sched::init(void) { return true; }

static void ktaskcreateret(void) {
  INFO("starting task '%s'\n", cpu::task()->name().get());

  cpu::popcli();

  // call the kernel task function
  cpu::task()->kernel_func();

  sched::exit();

  panic("ZOMBIE kernel task was ran\n");
}

pid_t sched::spawn_kernel_task(const char *name, void (*e)(),
                               create_opts opts) {
  // alloc the task under the kernel group
  auto p = talloc(name, 0, RING_KERNEL);

  // printk("eip: %p\n", e);
  p->context->eip = (u64)e;

  p->timeslice = opts.timeslice;
  //
  p->kernel_func = e;

  // the process is run in kernel mode
  p->tf->cs = (SEG_KCODE << 3);
  p->tf->ds = (SEG_KDATA << 3);
  p->tf->eflags = readeflags() | FL_IF;

  p->context->eip = (u64)ktaskcreateret;

  p->state = task::state::RUNNABLE;
  add_task(p);

  return p->pid();
}

static void switch_into(task *tsk) {
  INFO("ctxswtch: pid=%-4d gid=%-4d\n", tsk->pid(), tsk->gid());
  cpu::current().current_task = tsk;
  tsk->start_tick = cpu::get_ticks();
  tsk->state = task::state::RUNNING;

  swtch(&cpu::current().scheduler, tsk->context);
  cpu::current().current_task = nullptr;
}

static void schedule() {
  if (cpu::ncli() != 1) panic("schedule must have ncli == 1");
  int intena = cpu::current().intena;
  swtch(&cpu::task()->context, cpu::current().scheduler);
  cpu::current().intena = intena;
}

static void do_yield(enum task::state st) {
  INFO("yield %d\n", st);
  assert(cpu::task() != nullptr);

  cpu::pushcli();

  cpu::task()->state = st;
  schedule();

  cpu::popcli();
}

void sched::block() {
  do_yield(task::state::BLOCKED);
}

void sched::yield() { do_yield(task::state::RUNNABLE); }

void sched::exit() { do_yield(task::state::ZOMBIE); }

void sched::run() {
  for (;;) {
    auto tsk = next_task();

    assert(cpu::task() == nullptr);

    assert(tsk != nullptr);

    cpu::pushcli();

    bool add_back = true;

    if (tsk->state == task::state::RUNNABLE) {
      s_enabled = true;
      cpu::current().intena = 1;
      switch_into(tsk);
    }

    // check for state afterwards
    if (tsk->state == task::state::ZOMBIE) {
      // TODO: check for waiting processes and notify
      if (tsk->ring() == RING_KERNEL) {
        add_back = false;
        INFO("kernel '%s' task dead\n", tsk->name().get());
        delete tsk;
      }
    }

    cpu::popcli();

    if (add_back) {
      add_task(tsk);
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

void sched::beep(void) {
  play_tone(440, 25);
}

void sched::handle_tick(u64 ticks) {
  if (ticks >= beep_timeout) {
    pcspeaker::clear();
  }

  if (!enabled() || cpu::task() == nullptr ||
      cpu::task()->state != task::state::RUNNING) {
    return;
  }

  // yield?
  if (ticks - cpu::task()->start_tick >= cpu::task()->timeslice) {
    sched::yield();
  }
}
