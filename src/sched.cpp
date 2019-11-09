#include <asm.h>
#include <cpu.h>
#include <lock.h>
#include <map.h>
#include <pcspeaker.h>
#include <process.h>
#include <sched.h>
#include <single_list.h>
#include <wait.h>

// #define SCHED_DEBUG

#ifdef SCHED_DEBUG
#define INFO(fmt, args...) printk("[SCHED] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

extern "C" void swtch(context_t **, context_t *);

static mutex_lock process_lock("processes");
static bool s_enabled = true;

// the kernel process lives here, and should *never* die.
static pid_t kproc_pid = -1;
static mutex_lock proc_table_lock("process_table");
static map<pid_t, unique_ptr<process>> process_table;

static mutex_lock pid_lock("pid_lock");
pid_t sched::next_pid(void) {
  // the next pid starts at 2, since the kernel process is at pid 0, and the
  // init proc will be located at pid 1. We just hardcode that
  static int npid = 2;
  pid_lock.lock();
  pid_t n = npid++;
  pid_lock.unlock();
  return n;
}

process &sched::kernel_proc(void) {
  process *p = process_pid(kproc_pid);
  if (p == nullptr) panic("kernel proc is null!\n");
  return *p;
}

process *sched::process_pid(pid_t pid) {
  process_lock.lock();
  process *p = nullptr;

  // look for the process table entry
  if (process_table.contains(pid)) p = process_table.get(pid).get();
  process_lock.unlock();
  return p;
}

static thread *proc_queue;
static thread *last_proc;

static thread *next_thd(void) {
  thread *nt = nullptr;
  process_lock.lock();

  if (proc_queue != nullptr) {
    nt = proc_queue;
    proc_queue = nt->next;
  }
  process_lock.unlock();

  return nt;
}

static void add_thd(thread *tsk) {
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

bool sched::init(void) {
  process_lock.lock();

  // kernel proc is pid 0
  kproc_pid = 0;

  process_table[0] = new process("Essedarius", 0, 0, RING_KERNEL);

  process_lock.unlock();
  return true;
}

static void ktaskcreateret(void) {
  INFO("starting task '%s'\n", cpu::task()->name().get());

  cpu::popcli();

  // call the kernel task function
  cpu::thd().start();


  printk("KERNEL TASK DIES\n");

  sched::exit();

  panic("ZOMBIE kernel task was ran\n");
}

thread *sched::spawn_kernel_thread(const char *name, func<void(int)> fnc,
                                   create_opts opts) {
  // alloc the task under the kernel group
  /*
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
  */

  thread &t = kernel_proc().create_thread(fnc);

  t.timeslice = opts.timeslice;

  t.tf->cs = (SEG_KCODE << 3);
  t.tf->ds = (SEG_KDATA << 3);
  t.tf->eflags = readeflags() | FL_IF;

  t.context->eip = (u64)ktaskcreateret;
  t.state = pstate::RUNNABLE;

  add_thd(&t);

  return &t;
}

static void switch_into(thread *tsk) {
  INFO("ctxswtch: pid=%-4d gid=%-4d\n", tsk->pid(), tsk->gid());
  cpu::current().current_thread = tsk;
  tsk->nran++;
  tsk->start_tick = cpu::get_ticks();
  tsk->state = pstate::RUNNING;

  swtch(&cpu::current().scheduler, tsk->context);
  cpu::current().current_thread = nullptr;
}

static void schedule() {
  if (cpu::ncli() != 1) panic("schedule must have ncli == 1");
  int intena = cpu::current().intena;
  swtch(&cpu::thd().context, cpu::current().scheduler);
  cpu::current().intena = intena;
}

static void do_yield(enum pstate st) {
  INFO("yield %d\n", st);
  // assert(cpu::thd() != nullptr);

  cpu::pushcli();

  cpu::thd().state = st;
  schedule();

  cpu::popcli();
}

void sched::block() { do_yield(pstate::BLOCKED); }

void sched::yield() { do_yield(pstate::RUNNABLE); }

void sched::exit() {
  // printk("sched::exit\n");
  do_yield(pstate::ZOMBIE);
}

void sched::run() {
  for (;;) {
    auto tsk = next_thd();

    // assert(cpu::proc() == nullptr);

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
      if (tsk->proc().ring() == RING_KERNEL) {
        add_back = false;
        INFO("kernel '%s' process dead\n", tsk->name().get());
        delete tsk;
      }
    }

    cpu::popcli();

    if (add_back) {
      add_thd(tsk);
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

  if (!enabled() || !cpu::in_thread() || cpu::thd().state != pstate::RUNNING) {
    return;
  }

  // yield?
  if (ticks - cpu::thd().start_tick >= cpu::thd().timeslice) {
    // printk("yield\n");
    sched::yield();
  }
}


waitqueue::waitqueue(const char *name) : name(name), lock(name) {
}

void waitqueue::wait(void) {
  lock.lock();

  if (navail > 0) {
    navail--;
    lock.unlock();
    return;
  }

  assert(navail == 0);

  // add to the wait queue
  struct waitqueue_elem e;
  e.waiter = &cpu::thd();
  elems.append(e);
  lock.unlock();
  do_yield(pstate::BLOCKED);
}

void waitqueue::notify() {
  scoped_lock lck(lock);
  if (elems.is_empty()) {
    navail++;
  } else {
    auto e = elems.take_first();

    assert(e.waiter->state == pstate::BLOCKED);
    e.waiter->state = pstate::RUNNABLE;
  }
}
