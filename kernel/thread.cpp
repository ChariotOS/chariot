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




void dump_addr2line(void) {
  off_t rbp = 0;
  asm volatile("mov %%rbp, %0\n\t" : "=r"(rbp));
  auto bt = debug::generate_backtrace(rbp);
  printk_nolock("addr2line -e build/chariot.elf");
  for (auto pc : bt) {
    printk_nolock(" 0x%p", pc);
  }
  printk_nolock("\n");
}

static spinlock thread_table_lock;
static ck::map<long, Thread *> thread_table;

Thread::Thread(long tid, struct Process &proc) : proc(proc) {
  this->tid = tid;

  this->pid = proc.pid;

  fpu.initialized = false;
  fpu.state = phys::kalloc(1);

  sched.priority = 0;
  next = prev = nullptr;



  struct kernel_stack s;
  s.size = PGSIZE * 2;
  s.start = (void *)malloc(s.size);
  stacks.push(s);

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

  set_state(PS_EMBRYO);

  arch_reg(REG_SP, trap_frame) = sp;
  arch_reg(REG_PC, trap_frame) = -1;

  // set the initial context to the creation boostrap function
  kern_context->pc = (u64)thread_create_callback;


  arch_initialize_trapframe(proc.ring == RING_USER, trap_frame);
  {
    scoped_irqlock l(thread_table_lock);
    assert(!thread_table.contains(tid));
    thread_table.set(tid, this);
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
    assert(thread_table.contains(tid));
    // important: we cannot free() something while in an irqlock. As such, we must leak the pointer
    // from the table, then free it later.
    // thread_table.get(t->tid).leak_ptr();
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

ck::ref<Thread> Thread::lookup_r(long tid) { return thread_table.get(tid); }

ck::ref<Thread> Thread::lookup(long tid) {
  scoped_irqlock l(thread_table_lock);
  assert(thread_table.contains(tid));
  auto t = Thread::lookup_r(tid);
  return t;
}



void Thread::dump(void) {
  scoped_irqlock l(thread_table_lock);
  for (auto &[tid, twr] : thread_table) {
    ck::ref<Thread> thd = twr;
    printk_nolock("t:%d p:%d : %p %d refs\n", tid, thd->pid, thd.get(), thd->ref_count());
  }
}


bool Thread::teardown(ck::ref<Thread> &&thd) {
#ifdef CONFIG_VERBOSE_PROCESS
  pprintk("thread ran for %llu cycles, %llu us\n", thd->stats.cycles, thd->ktime_us);
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

#ifdef CONFIG_VERBOSE_PROCESS
  printk("sending signal %d to tid %d\n", sig, tid);
#endif

  bool f;

  unsigned long pend = (1 << sig);
  this->sig.pending |= pend;

  if (state == PS_INTERRUPTIBLE) this->interrupt();

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
int sys::spawnthread(void *stack, void *fn, void *arg, int flags) {
  int tid = get_next_pid();
  auto thd = ck::make_ref<Thread>(tid, *curproc);

  arch_reg(REG_SP, thd->trap_frame) = (unsigned long)stack;
  arch_reg(REG_PC, thd->trap_frame) = (unsigned long)fn;
  arch_reg(REG_ARG0, thd->trap_frame) = (unsigned long)arg;

  thd->kickoff(fn, PS_RUNNING);

  return tid;
}


bool Thread::join(ck::ref<Thread> thd) {
  {
    /* take the join lock */
    scoped_lock joinlock(thd->joinlock);
    if (thd->state != PS_ZOMBIE) {
      if (thd->joiners.wait().interrupted()) {
        return false;
      }
    }
  }

  // take the run lock
  thd->runlock.lock();
  Thread::teardown(move(thd));
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

  Thread::join(move(t));
  return 0;
}


void Thread::set_state(int st) {
  // store with sequential consistency
  __atomic_store(&this->state, &st, __ATOMIC_SEQ_CST);
}

int Thread::get_state(void) { return __atomic_load_n(&this->state, __ATOMIC_ACQUIRE); }
