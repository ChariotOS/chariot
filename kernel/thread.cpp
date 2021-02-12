/**
 * This file implements all the thread:: functions and methods
 */
#include <cpu.h>
#include <mmap_flags.h>
#include <phys.h>
#include <sched.h>
#include <syscall.h>
#include <time.h>
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

static rwlock thread_table_lock;
static map<pid_t, struct thread *> thread_table;

thread::thread(pid_t tid, struct process &proc) : proc(proc) {
  this->tid = tid;

  this->pid = proc.pid;

  fpu.initialized = false;
  fpu.state = phys::kalloc(1);

  sched.priority = PRIORITY_HIGH;



  struct thread::kernel_stack s;
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

  state = PS_EMBRYO;

  arch_reg(REG_SP, trap_frame) = sp;
  arch_reg(REG_PC, trap_frame) = -1;

  // set the initial context to the creation boostrap function
  kern_context->pc = (u64)thread_create_callback;


  arch_initialize_trapframe(proc.ring == RING_USER, trap_frame);

  thread_table_lock.write_lock();
  assert(!thread_table.contains(tid));
  // printk("inserting %d\n", tid);
  thread_table.set(tid, this);
  thread_table_lock.write_unlock();


  // push the tid into the proc's tid list
  proc.threads.push(tid);


#if CONFIG_VERBOSE_PROCESS
  printk("Thread %d:%d created\n", pid, tid);
#endif
}


off_t thread::setup_tls(void) {
  // Map in the TLS if there is one
  // TODO maybe move TLS to userspace?
  if (proc.tls_info.exists) {
    tls_usize = proc.tls_info.memsz;
    tls_uaddr = proc.mm->mmap(string::format("[tid %d TLS]", this->tid), 0, proc.tls_info.memsz,
                              PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, nullptr, 0);

    printk("============================== %p\n", tls_uaddr);
    proc.mm->dump();
  } else {
    tls_uaddr = 0;
  }

  return tls_uaddr;
}


bool thread::kickoff(void *rip, int initial_state) {
  arch_reg(REG_PC, trap_frame) = (unsigned long)rip;

  this->state = initial_state;

  setup_tls();

  sched::add_task(this);
  return true;
}


thread::~thread(void) {
  if (tls_uaddr != 0) {
    proc.mm->dump();
    proc.mm->unmap(tls_uaddr, tls_usize);
  }

  sched::remove_task(this);

  /* Remove this thread from the parent process */
  this->proc.threads.remove_first_matching([this](int otid) {
    /* If the thread id in the vector is the current tid, remove it */
    return otid == this->tid;
  });

  for (auto &s : stacks) {
    free(s.start);
  }
  // free(stack);
  phys::free(fpu.state, 1);
  // assume it doesn't have a destructor, idk
  free(sig.arch_priv);
}


void thread::setup_stack(reg_t *tf) {
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



static void thread_create_callback(void *) {
  arch_thread_create_callback();
}

struct thread *thread::lookup(pid_t tid) {
  thread_table_lock.read_lock();
  assert(thread_table.contains(tid));
  auto t = thread_table.get(tid);
  thread_table_lock.read_unlock();
  return t;
}

bool thread::teardown(thread *t) {
#ifdef CONFIG_VERBOSE_PROCESS
  printk("thread ran for %llu cycles, %llu us\n", t->stats.cycles, t->ktime_us);
#endif

  thread_table_lock.write_lock();
  assert(thread_table.contains(t->tid));
  thread_table.remove(t->tid);
  thread_table_lock.write_unlock();

  delete t;
  return true;
}



bool thread::send_signal(int sig) {
  bool is_self = curthd == this;

  unsigned long pend = (1 << sig);
  this->sig.pending |= pend;
#ifdef CONFIG_VERBOSE_PROCESS
  printk("sending signal %d to tid %d\n", sig, tid);
#endif

  bool f;
  if (!is_self) f = locks.run.lock_irqsave();
  if (state == PS_INTERRUPTIBLE) {
    this->interrupt();
  }
  if (!is_self) locks.run.unlock_irqrestore(f);
  return true;
}



vec<off_t> thread::backtrace(off_t rbp, off_t rip) {
  vec<off_t> bt;
  bt.push(rip);

  for (auto sp = (off_t *)rbp; VALIDATE_RD(sp, sizeof(off_t) * 2); sp = (off_t *)sp[0]) {
    auto retaddr = sp[1];
    if (!VALIDATE_EXEC((void *)retaddr, sizeof(off_t))) break;
    bt.push(retaddr);
  }

  return bt;
}

void thread::interrupt(void) {
  sched::unblock(*this, true);
}


extern int get_next_pid(void);

// TODO: alot of verification, basically
int sys::spawnthread(void *stack, void *fn, void *arg, int flags) {
  int tid = get_next_pid();
  auto *thd = new thread(tid, *curproc);

  arch_reg(REG_SP, thd->trap_frame) = (unsigned long)stack;
  arch_reg(REG_PC, thd->trap_frame) = (unsigned long)fn;
  arch_reg(REG_ARG0, thd->trap_frame) = (unsigned long)arg;

  thd->kickoff(fn, PS_RUNNING);

  return tid;
}

int sys::jointhread(int tid) {
  auto *t = thread::lookup(tid);
  /* If there isn't a thread, fail */
  if (t == NULL) return -ENOENT;
  /* You can't join on other proc's threads */
  if (t->pid != curthd->pid) return -EPERM;
  /* If someone else is joining, it's invalid */
  if (t->joinlock.is_locked()) return -EINVAL;

  /* take the join lock */
  {
    scoped_lock joinlock(t->joinlock);

    if (t->state != PS_ZOMBIE) {
      if (t->joiners.wait().interrupted()) {
        return -EINTR;
      }
    }
    // take the run lock
    t->locks.run.lock();
    thread::teardown(t);
  }
  return 0;
}
