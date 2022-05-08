/**
 * This file implements all the thread:: functions and methods
 */

#include <thread.h>


#include <cpu.h>
#include <mmap_flags.h>
#include <phys.h>
#include <syscall.h>
#include <time.h>
#include <debug.h>
#include <module.h>
#include <util.h>

/**
 * A thread needs to bootstrap itself somehow, and it uses this function to do
 * so. Both kinds of threads (user and kernel) start by executing this function.
 * User threads will iret to userspace, and kernel threads will simply call it's
 * function then exit when the function returns
 */
static void thread_create_callback(void *);
// implemented in arch/$ARCH/trap.asm most likely
extern "C" void trapret(void);




// void dump_addr2line(void) {
//   off_t rbp = 0;
//   asm volatile("mov %%rbp, %0\n\t" : "=r"(rbp));
//   auto bt = debug::generate_backtrace(rbp);
//   printf_nolock("addr2line -e build/chariot.elf");
//   for (auto pc : bt) {
//     printf_nolock(" 0x%p", pc);
//   }
//   printf_nolock("\n");
// }

static spinlock thread_table_lock;
ck::map<long, ck::weak_ref<Thread>> thread_table;

Thread::Thread(long tid, Process &proc) : proc(proc) {
  this->tid = tid;
  this->pid = proc.pid;

  fpu.initialized = false;
  fpu.state = phys::kalloc(1);

  sched.priority = 0;
  next = prev = nullptr;

  KernelStack s;
  s.size = PGSIZE * 2;
  s.start = (void *)malloc(s.size);
  stacks.push(s);

  name = proc.name;

  auto sp = (off_t)s.start + s.size;

  // get a pointer to the trapframe on the stack
  sp -= arch_trapframe_size();
  trap_frame = (reg_t *)sp;

  // 'return' to trapret()
  sp -= sizeof(void *);
  *(void **)sp = (void *)trapret;

  // initial kernel context
  sp -= sizeof(struct thread_context);
  kern_context = (struct thread_context *)sp;
  memset(kern_context, 0, sizeof(struct thread_context));

  arch_reg(REG_SP, trap_frame) = sp;
  arch_reg(REG_BP, trap_frame) = sp;
  arch_reg(REG_PC, trap_frame) = -1;

  // set the initial context to the creation boostrap function
  kern_context->pc = (u64)thread_create_callback;
  // return from interrupt will drop this to 0
  irq_depth = 0;


  arch_initialize_trapframe(proc.ring == RING_USER, trap_frame);
  {
    scoped_irqlock l(thread_table_lock);
    assert(!thread_table.contains(tid));
    ck::ref<Thread> t = this;
    thread_table.set(tid, t);
  }

  {
    scoped_irqlock l(proc.threads_lock);
    proc.threads.push(this);
  }
}

Thread::~Thread(void) {
  assert(ref_count() == 0);
  {
    scoped_irqlock l(thread_table_lock);
    // assert(thread_table.contains(tid));
    thread_table.remove(tid);
  }

  // free all the kernel stacks
  for (auto &s : stacks) {
    free(s.start);
  }
  // free the FPU state page
  phys::free(fpu.state, 1);

  // free the architecture specific state for this thread.
  if (sig.arch_priv != NULL) free(sig.arch_priv);

  // memset((void *)this, 0xFF, sizeof(*this));
}




bool Thread::kickoff(void *rip, int initial_state) {
  arch_reg(REG_PC, trap_frame) = (unsigned long)rip;

  this->state = initial_state;

  sched::add_task(this);
  return true;
}




void Thread::setup_stack(reg_t *tf) {
  auto sp = arch_reg(REG_SP, tf);
#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define STACK_ALLOC(T, n)               \
  ({                                    \
    sp -= round_up(sizeof(T) * (n), 8); \
    (T *)(void *) sp;                   \
  })
  auto argc = (u64)proc.args.size();

  size_t sz = 0;
  sz += proc.args.size() * sizeof(char *);
  for (auto &a : proc.args)
    sz += a.size() + 1;

  auto region = (void *)STACK_ALLOC(char, sz);

  auto argv = (char **)region;

  auto *arg = (char *)((char **)argv + argc);

  int i = 0;
  for (auto &a : proc.args) {
    int len = a.size() + 1;
    memcpy(arg, a.get(), len);
    argv[i++] = arg;
    arg += len;
  }

  auto envc = proc.env.size();
  auto envp = STACK_ALLOC(char *, envc + 1);

  for (int i = 0; i < envc; i++) {
    auto &e = proc.env[i];
    envp[i] = STACK_ALLOC(char, e.len() + 1);
    memcpy(envp[i], e.get(), e.len() + 1);
  }

  envp[envc] = NULL;


  // align the stack to 16 bytes. (this is what intel wants, so it what I
  // will give them)
  arch_reg(REG_SP, tf) = sp & ~0xF;
  arch_reg(REG_BP, tf) = sp & ~0xF;
#ifdef CONFIG_X86
  tf[1] = argc;
  tf[2] = (unsigned long)argv;
  tf[3] = (unsigned long)envp;
#endif

#ifdef CONFIG_RISCV
  rv::regs *regs = (rv::regs *)tf;
  regs->a0 = argc;
  regs->a1 = (unsigned long)argv;
  regs->a2 = (unsigned long)envp;
#endif
}



static void thread_create_callback(void *) { arch_thread_create_callback(); }

ck::ref<Thread> Thread::lookup_r(long tid) { return thread_table.get(tid).get(); }

ck::ref<Thread> Thread::lookup(long tid) {
  scoped_irqlock l(thread_table_lock);
  assert(thread_table.contains(tid));
  auto t = Thread::lookup_r(tid);
  return t;
}



void Thread::dump(void) {
  scoped_irqlock l(thread_table_lock);
  for (auto &[tid, twr] : thread_table) {
    ck::ref<Thread> thd = twr.get();
    auto pc = arch_reg(REG_PC, thd->trap_frame);
    const char *state_string = "unknown";

    switch (thd->state) {
      case PS_RUNNING:
        state_string = "running";
        break;

      case PS_INTERRUPTIBLE:
        state_string = "blocked (int)";
        break;

      case PS_UNINTERRUPTIBLE:
        state_string = "blocked (noint)";
        break;

      case PS_ZOMBIE:
        state_string = "blocked (noint)";
        break;
    }
    printf_nolock(
        "t:%3d, p:%3d, %s, pc:%p, st:%s, e:%d, irqd:%d\n", tid, thd->pid, thd->name.get(), pc, state_string, thd->kerrno, thd->irq_depth);
    // printf_nolock("t:%d p:%d : %p %d refs\n", tid, thd->pid, thd.get(), thd->ref_count());
  }
}



ksh_def("threads", "display all the threads in great detail") {
  Thread::dump();
  return 0;
}


void Thread::exit(void) {
  should_die = 1;
  sched::unblock(*this, true);
}

bool Thread::teardown(ck::ref<Thread> &&thd) {
#ifdef CONFIG_VERBOSE_PROCESS
  pprintf("thread ran for %llu cycles, %llu us\n", thd->stats.cycles, thd->ktime_us);
#endif

  {
    scoped_irqlock l(thd->proc.threads_lock);

    thd->proc.threads.remove_first_matching([&](auto &o) {
      return o->tid == thd->tid;
    });
  }

  sched::remove_task(thd);
  thd = nullptr;

  return true;
}



bool Thread::send_signal(int sig) {
  bool is_self = curthd == this;


  bool f;
  if (!is_self) f = runlock.lock_irqsave();

#ifdef CONFIG_VERBOSE_PROCESS
  printf("sending signal %d to tid %d\n", sig, tid);
#endif

  unsigned long pend = (1 << sig);
  this->sig.pending |= pend;
  if (get_state() == PS_INTERRUPTIBLE) this->interrupt();

  if (!is_self) runlock.unlock_irqrestore(f);
  return true;
}



ck::vec<off_t> Thread::backtrace(off_t rbp, off_t rip) {
  ck::vec<off_t> bt;
  bt.push(rip);

  for (auto sp = (off_t *)rbp; VALIDATE_RD(sp, sizeof(off_t) * 2); sp = (off_t *)sp[0]) {
    auto retaddr = sp[1];
    if (!VALIDATE_EXEC((void *)retaddr, sizeof(off_t))) break;
    bt.push(retaddr);
  }

  return bt;
}

void Thread::interrupt(void) { sched::unblock(*this, true); }


extern int get_next_pid(void);

// TODO: alot of verification, basically
int sys::spawnthread(void *vstack, void *fn, void *arg, int flags) {
  int tid = get_next_pid();
  auto thd = ck::make_ref<Thread>(tid, *curproc);


  memset(thd->trap_frame, 0, arch_trapframe_size());
  arch_initialize_trapframe(true, thd->trap_frame);

  auto stack = (unsigned long)vstack;

  stack -= 24;
  arch_reg(REG_SP, thd->trap_frame) = stack;
  arch_reg(REG_BP, thd->trap_frame) = stack;
  arch_reg(REG_PC, thd->trap_frame) = (unsigned long)fn;
  arch_reg(REG_ARG0, thd->trap_frame) = (unsigned long)arg;

  thd->kickoff(fn, PS_RUNNING);

  return tid;
}


bool Thread::join(ck::ref<Thread> thd) {
  panic("oh no\n");
  return true;
}

int sys::jointhread(int tid) {
  auto t = Thread::lookup(tid);
  /* If there isn't a thread, fail */
  if (t == nullptr) return -ENOENT;
  /* You can't join on other proc's threads */
  if (t->pid != curthd->pid) return -EPERM;
  /* If someone else is joining, it's invalid */
  if (t->joinlock.is_locked()) return -EINVAL;


  // take the run lock
  // thd->runlock.lock();
  while (1) {
    /* take the join lock */

    auto f = t->joinlock.lock_irqsave();
    if (t->get_state() == PS_ZOMBIE) {
      t->joinlock.unlock_irqrestore(f);
      t->runlock.lock();
      Thread::teardown(move(t));
      return 0;
    }

    // wait!
    wait_entry ent;
    prepare_to_wait(t->joiners, ent, true);
    t->joinlock.unlock_irqrestore(f);
    if (ent.start().interrupted()) return -EINTR;
    // sched::yield();
  }
}


void Thread::set_state(int st) {
  // store with sequential consistency
  __atomic_store(&this->state, &st, __ATOMIC_SEQ_CST);
}

int Thread::get_state(void) { return __atomic_load_n(&this->state, __ATOMIC_ACQUIRE); }




// Threads are assumed preemptable
bool Thread::rt_is_preemptable(void) { return this->preemptable; }



// code to switch into a thread
extern "C" void context_switch(struct thread_context **, struct thread_context *);
void Thread::run(void) {
  if (held_lock != NULL) {
    if (!held_lock->try_lock()) return;
  }
  runlock.lock();

  // auto start = time::now_us();

  cpu::current().current_thread = this;
  cpu::current().next_thread = nullptr;

  barrier();

  // update the statistics of the thread
  stats.run_count++;
  stats.current_cpu = cpu::current().id;
  state = PS_RUNNING;

  if (proc.ring == RING_USER) arch_restore_fpu(*this);

  sched.has_run = 0;

  barrier();

  // load up the thread's address space
  cpu::switch_vm(this);

  barrier();

  stats.last_start_cycle = arch_read_timestamp();
  bool ts = time::stabilized();
  long start_us = 0;
  if (ts) {
    start_us = time::now_us();
  }

  barrier();
  // Switch into the thread!
  context_switch(&cpu::current().sched_ctx, kern_context);
  barrier();

  if (proc.ring == RING_USER) arch_save_fpu(*this);

  if (ts) this->ktime_us += time::now_us() - start_us;

  // Update the stats afterwards
  cpu::current().current_thread = nullptr;

  // printf_nolock("took %llu us\n", time::now_us() - start);
  this->runlock.unlock();
  if (this->held_lock != NULL) this->held_lock->unlock();

  // TODO: do realtime config
}
