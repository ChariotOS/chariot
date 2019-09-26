#include <asm.h>
#include <cpu.h>
#include <lock.h>
#include <pcspeaker.h>
#include <process.h>
#include <sched.h>
#include <single_list.h>

// #define SCHED_DEBUG

#ifdef SCHED_DEBUG
#define INFO(fmt, args...) printk("[SCHED] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

extern "C" void swtch(context_t **, context_t *);

static mutex_lock process_lock("processes");
int next_pid = 1;
static bool s_enabled = true;

static process *proc_queue;
static process *last_proc;

static process *proc_alloc(const char *name, gid_t group, int ring) {
  process_lock.lock();
  auto t = new process(name, next_pid++, group, ring);
  process_lock.unlock();
  return t;
}

static process *next_task(void) {
  process *nt = nullptr;
  process_lock.lock();

  if (proc_queue != nullptr) {
    nt = proc_queue;
    proc_queue = nt->next;
  }
  process_lock.unlock();

  return nt;
}

static void add_task(process *tsk) {
  process_lock.lock();

  if (proc_queue == nullptr) {
    proc_queue = tsk;
    last_proc = tsk;
  } else {
    last_proc->next = tsk;
    last_proc = tsk;
  }
  process_lock.unlock();
}

bool sched::init(void) { return true; }

static void ktaskcreateret(void) {
  INFO("starting task '%s'\n", cpu::task()->name().get());

  cpu::popcli();

  // call the kernel task function
  cpu::proc()->kernel_func();

  sched::exit();

  panic("ZOMBIE kernel task was ran\n");
}

pid_t sched::spawn_kernel_thread(const char *name, void (*e)(),
                               create_opts opts) {
  // alloc the task under the kernel group
  auto p = proc_alloc(name, 0, RING_KERNEL);

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

  p->state = pstate::RUNNABLE;
  add_task(p);

  KINFO("spawned kernel task '%s' pid=%d\n", name, p->pid());

  return p->pid();
}

static void switch_into(process *tsk) {
  INFO("ctxswtch: pid=%-4d gid=%-4d\n", tsk->pid(), tsk->gid());
  cpu::current().current_proc = tsk;
  tsk->start_tick = cpu::get_ticks();
  tsk->state = pstate::RUNNING;

  swtch(&cpu::current().scheduler, tsk->context);
  cpu::current().current_proc = nullptr;
}

static void schedule() {
  if (cpu::ncli() != 1) panic("schedule must have ncli == 1");
  int intena = cpu::current().intena;
  swtch(&cpu::proc()->context, cpu::current().scheduler);
  cpu::current().intena = intena;
}

static void do_yield(enum pstate st) {
  INFO("yield %d\n", st);
  assert(cpu::proc() != nullptr);

  cpu::pushcli();

  cpu::proc()->state = st;
  schedule();

  cpu::popcli();
}

void sched::block() { do_yield(pstate::BLOCKED); }

void sched::yield() { do_yield(pstate::RUNNABLE); }

void sched::exit() { do_yield(pstate::ZOMBIE); }

void sched::run() {
  for (;;) {
    auto tsk = next_task();

    assert(cpu::proc() == nullptr);

    assert(tsk != nullptr);

    cpu::pushcli();

    bool add_back = true;

    if (tsk->state == pstate::RUNNABLE) {
      s_enabled = true;
      cpu::current().intena = 1;
      // printk("switching into %s\n", tsk->name().get());
      switch_into(tsk);
    } else {
      // printk("proc %s was not runnable\n", tsk->name().get());
    }

    // check for state afterwards
    if (tsk->state == pstate::ZOMBIE) {
      // TODO: check for waiting processes and notify
      if (tsk->ring() == RING_KERNEL) {
        add_back = false;
        INFO("kernel '%s' process dead\n", tsk->name().get());
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

void sched::beep(void) { play_tone(440, 25); }

void sched::handle_tick(u64 ticks) {
  if (ticks >= beep_timeout) {
    pcspeaker::clear();
  }

  if (!enabled() || cpu::proc() == nullptr ||
      cpu::proc()->state != pstate::RUNNING) {
    return;
  }

  // yield?
  if (ticks - cpu::proc()->start_tick >= cpu::proc()->timeslice) {
    sched::yield();
  }
}
