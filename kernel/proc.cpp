/**
 * This file implements process functions from sched::proc::*
 * in include/sched.h
 */

#include <cpu.h>
#include <elf/loader.h>
#include <errno.h>
#include <fs.h>
#include <fs/vfs.h>
#include <futex.h>
#include <lock.h>
#include <mem.h>
#include <paging.h>
#include <phys.h>
#include <sched.h>
#include <syscall.h>
#include <util.h>
#include <wait_flags.h>

#ifdef CONFIG_RISCV
#include <riscv/arch.h>
#endif

// start out at pid 2, so init is pid 1 regardless of if kernel threads are
// created before init is spawned
volatile long next_pid = 2;

// static rwlock ptable_lock;
static spinlock ptable_lock;
static ck::map<long, process::ptr> proc_table;

long get_next_pid(void) { return __atomic_add_fetch(&next_pid, 1, __ATOMIC_ACQUIRE); }

mm::space *alloc_user_vm(void) {
  unsigned long top = 0x7ff000000000;
#ifdef CONFIG_SV39
  top = 0x3ff0000000;
#endif

  top -= rand() & 0xFFFFF000;
  return new mm::space(0x1000, top, mm::pagetable::create());
}

static process::ptr pid_lookup(long pid) {
  scoped_irqlock l(ptable_lock);
  process::ptr p = nullptr;
  if (proc_table.contains(pid)) {
    p = proc_table.get(pid);
  }

  return p;
}



void sched::proc::in_pgrp(long pgid, ck::func<bool(struct process &)> cb) {
  scoped_irqlock l(ptable_lock);
  for (auto kv : proc_table) {
    auto proc = kv.value;
    if (proc->pgid == pgid) {
      if (cb(*proc) == false) break;
    }
  }
}

static process::ptr do_spawn_proc(process::ptr proc_ptr, int flags) {
  // get a reference, they are nicer to look at.
  // (and we spend less time in ck::ref<process>::{ref,deref}())
  auto &proc = *proc_ptr;


  scoped_lock lck(proc.datalock);
  proc.create_tick = cpu::get_ticks();

  proc.ring = flags & SPAWN_KERN ? RING_KERN : RING_USER;

  // This check is needed because the kernel has no parent.
  if (proc.parent) {
    proc.user = proc.parent->user;  // inherit the user information
    proc.ppid = proc.parent->pid;
    proc.pgid = proc.parent->pgid;  // inherit the group id of the parent

    proc.root = geti(proc.parent->root);
    proc.cwd = geti(proc.parent->cwd);

    /* copy the signal structure */
    proc.sig = proc.parent->sig;


    // are we forking?
    if (flags & SPAWN_FORK) {
      // inherit all file descriptors
      proc.parent->file_lock.lock();
      for (auto &kv : proc.parent->open_files) {
        proc.open_files[kv.key] = kv.value;
      }
      proc.parent->file_lock.unlock();

    } else {
      // inherit stdin(0) stdout(1) and stderr(2)
      for (int i = 0; i < 3; i++) {
        proc.open_files[i] = proc.parent->get_fd(i);
      }
    }
  }

  if (proc.cwd == NULL) {
    proc.cwd = vfs::get_root();
  }
  // special case for kernel's init
  if (proc.cwd != NULL) geti(proc.cwd);


  if (flags & SPAWN_FORK) {
    proc.mm = proc.parent->mm->fork();
  } else {
    proc.mm = alloc_user_vm();
  }
  return proc_ptr;
}

process::ptr sched::proc::spawn_process(struct process *parent, int flags) {
  // grab the next pid (will be the tid of the main thread when it is executed)
  long pid = get_next_pid();


  process::ptr proc;
  proc = ck::make_ref<process>();
  // allocate the process
  {
    scoped_irqlock l(ptable_lock);
    proc_table.set(pid, proc);
  }
  if (parent != NULL) {
    proc->name = parent->name;
  }

  proc->pid = pid;
  proc->pgid = parent == NULL ? pid : parent->pgid;  // pgid == pid (if no parent)

  // Set up initial data for the process.
  proc->parent = parent;
  return do_spawn_proc(proc, flags);
};

bool sched::proc::ptable_remove(long p) {
  scoped_irqlock l(ptable_lock);
  bool succ = false;

  if (proc_table.contains(p)) {
    proc_table.remove(p);
    succ = true;
  }

  return succ;
}

long sched::proc::spawn_init(ck::vec<ck::string> &paths) {
  long pid = 1;

  process::ptr proc_ptr;
  proc_ptr = ck::make_ref<process>();
  if (proc_ptr.get() == NULL) {
    return -1;
  }

  proc_ptr->pid = pid;
  proc_ptr->pgid = pid;  // pgid == pid (if no parent)

  {
    scoped_irqlock l(ptable_lock);
    assert(!proc_table.contains(pid));
    proc_table.set(pid, proc_ptr);
  }

  proc_ptr->parent = sched::proc::kproc();

  // initialize normally
  proc_ptr = do_spawn_proc(proc_ptr, 0);

  // init starts in the root directory
  proc_ptr->root = geti(vfs::get_root());
  proc_ptr->cwd = geti(vfs::get_root());
  auto &proc = *proc_ptr;

  ck::vec<ck::string> envp;
  for (auto &path : paths) {
    ck::vec<ck::string> argv;
    argv.push(path);
    int res = proc.exec(path, argv, envp);
    if (res == 0) return pid;
  }

  return -1;
}



static process::ptr kernel_process = nullptr;


struct process *sched::proc::kproc(void) {
  if (kernel_process.get() == nullptr) {
    // spawn the kernel process
    kernel_process = sched::proc::spawn_process(nullptr, SPAWN_KERN);
    kernel_process->pid = 0;  // just lower than init (pid 1)
    kernel_process->name = "kernel";
    // auto &vm = kernel_process->mm;
    delete kernel_process->mm;
    kernel_process->mm = &mm::space::kernel_space();
  }

  return kernel_process.get();
}


ck::ref<thread> sched::proc::spawn_kthread(const char *name, int (*func)(void *), void *arg) {
  auto proc = kproc();
  auto tid = get_next_pid();
  auto thd = ck::make_ref<thread>(tid, *proc);
  thd->trap_frame[1] = (unsigned long)arg;


  arch_reg(REG_PC, thd->trap_frame) = (unsigned long)func;
  thd->set_state(PS_RUNNING);

  return thd;
}


long sched::proc::create_kthread(const char *name, int (*func)(void *), void *arg) {
  auto proc = kproc();

  auto tid = get_next_pid();

  // construct the thread
  auto thd = new thread(tid, *proc);
  thd->trap_frame[1] = (unsigned long)arg;

  thd->kickoff((void *)func, PS_RUNNING);

  KINFO("Created kernel thread '%s'. tid=%d\n", name, tid);

  return tid;
}

ck::ref<fs::file> process::get_fd(int fd) {
  ck::ref<fs::file> file;

  scoped_lock l(file_lock);
  auto it = open_files.find(fd);
  if (it != open_files.end()) {
    file = it->value;
  }

  return file;
}

int process::add_fd(ck::ref<fs::file> file) {
  int fd = 0;
  scoped_lock l(file_lock);

  while (1) {
    if (!open_files.contains(fd)) break;
    fd++;
  }

  open_files.set(fd, file);
  return fd;
}

process::~process(void) {
  if (threads.size() != 0) {
    KWARN("destruction of proc %d still has threads!\n", this->pid);
  }

  sched::proc::ptable_remove(this->pid);
  delete mm;
}

bool process::is_dead(void) {
  for (auto t : threads) {
    assert(t);

    if (t->get_state() != PS_ZOMBIE) {
      return false;
    }
  }

  return true;
}

int process::exec(ck::string &path, ck::vec<ck::string> &argv, ck::vec<ck::string> &envp) {
  scoped_lock lck(this->datalock);


  // try to load the binary
  fs::inode *exe = NULL;

  // TODO: open permissions on the binary
  if (vfs::namei(path.get(), 0, 0, cwd, exe) != 0) {
    return -ENOENT;
  }
  // TODO check execution permissions

  off_t entry = 0;
  auto fd = ck::make_ref<fs::file>(exe, FDIR_READ);

  // allocate a new address space
  auto *new_addr_space = alloc_user_vm();


  int loaded = elf::load(path.get(), *this, *new_addr_space, fd, entry);

  if (loaded != 0) {
    delete new_addr_space;
    return -ENOEXEC;
  }

  off_t stack = 0;
  // allocate a 1mb stack
  // TODO: this size is arbitrary.
  auto stack_size = 1024 * 1024;

  stack = new_addr_space->mmap("[stack]", 0, stack_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, nullptr, 0);

  // TODO: push argv and arguments onto the stack
  this->args = argv;
  this->env = envp;
  delete this->mm;
  this->mm = new_addr_space;

  ck::ref<thread> thd;
  if (threads.size() != 0) {
    thd = thread::lookup(this->pid);
  } else {
    thd = ck::make_ref<thread>(pid, *this);
  }

  printk("hello\n");

  // construct the thread
  arch_reg(REG_SP, thd->trap_frame) = stack + stack_size - 64;
  thd->kickoff((void *)entry, PS_RUNNING);

  return 0;
}

int sched::proc::send_signal(long p, int sig) {
  // TODO: handle process group signals
  if (p < 0) {
    int err = -ESRCH;
    long pgid = -p;
    ck::vec<long> targs;
    sched::proc::in_pgrp(pgid, [&](struct process &p) -> bool {
      targs.push(p.pid);
      return true;
    });

    for (long pid : targs)
      sched::proc::send_signal(pid, sig);

    return 0;
  }

  if (sig < 0 || sig >= 64) {
    return -EINVAL;
  }

  int err = -ESRCH;
  {
    auto targ = pid_lookup(p);

    if (targ) {
      // find a thread
      for (auto thd : targ->threads) {
        if (thd->send_signal(sig)) {
          err = 0;
          break;
        }
      }
    }
  }

  return err;
}

int sched::proc::reap(process::ptr p) {
  assert(p->is_dead());
  auto *me = curproc;


  int f = 0;

  f |= (p->exit_signal & 0xFF);
  f |= (p->exit_code & 0xFF) << 8;

#ifdef CONFIG_VERBOSE_PROCESS
  pprintk("reap (p:%d, pg:%d) on cpu %d\n", p->pid, p->pgid, cpu::current().cpunum);
  auto usage = p->mm->memory_usage();
  pprintk("  ram usage: %zu Kb (%zu b)\n", usage / KB, usage);
#endif


  {
    ck::vec<ck::ref<thread>> to_teardown = p->threads;

    for (auto t : to_teardown) {
      assert(t->get_state() == PS_ZOMBIE);
      /* make sure... */
      t->locks.run.lock();
      thread::teardown(move(t));
    }
  }
  assert(p->threads.size() == 0);


  /* remove the child from our list */
  for (int i = 0; i < me->children.size(); i++) {
    if (me->children[i] == p) {
      me->children.remove(i);
      break;
    }
  }

  // release the CWD and root
  fs::inode::release(p->cwd);
  fs::inode::release(p->root);


  // If the process had children, we need to give them to init
  if (p->children.size() != 0) {
    // grab a pointer to init
    process::ptr init = pid_lookup(1);
    assert(init);
    init->datalock.lock();
    for (auto &c : p->children) {
      init->children.push(c);
      c->parent = init;
    }
    init->datalock.unlock();
  }

  ptable_remove(p->pid);

  return f;
}


void process::terminate(int signal) {
  pprintk("Terminate with signal %d\n", signal);
  signal &= 0x7F;
  /* top bit of the first byte means it was signalled. Everything else is the signal that killed it
   */
  this->exit_signal = signal | 0x80;
  sys::exit_proc(signal + 128);
}




/*
 * The waitpid subsystem consists of two lists: Zombies and Waiters. When a process wants become a
 * zombie (exit), it first looks through waiting threads to see if anyone is interested in reaping
 * them. If no thread is available to reap it, it creates an entry in the zombie_list for future
 * waiters to manage.
 */
static spinlock waitlist_lock;
static list_head zombie_list = LIST_HEAD_INIT(zombie_list);  // locked by waitlist_lock
static list_head waiter_list = LIST_HEAD_INIT(waiter_list);  // locked by waitlist_lock

struct zombie_entry {
  // an entry in the zombie_list
  struct list_head node;
  // the zombie process
  process::ptr proc;
  zombie_entry(process::ptr proc) : proc(proc) { node.init(); }
  ~zombie_entry(void) {}
};

struct waiter_entry {
  // an entry in the waiter_list
  struct list_head node;
  long seeking;  // the pid that this waiter is interested in (waitpid systemcall semantics)
  // systemcall wait flags
  int wait_flags = 0;
  // the process that we caught
  process::ptr proc;
  // the waiting thread
  struct thread *thd;
  // the waitqueue that the process sits on
  wait_queue wq;
};




static bool can_reap(struct thread *reaper, process::ptr zombie, long seeking, int reap_flags) {
  if (!zombie->parent) return false;
  // a process may not reap another process's children.
  if (zombie->parent->pid != reaper->proc.pid) return false;

  if (zombie->pid == seeking) {
    return true;
  }

  // if you seek -1, and the process is your child, you can reap it
  if (seeking == -1) {
    return true;
  }


  // if the pid is less than -1, you are actually waiting on a process group
  if (seeking < -1 && zombie->pgid == -seeking) {
    return true;
  }

  // all checks have failed
  return false;
}




int sched::proc::do_waitpid(long pid, int &status, int wait_flags) {
  auto *me = curproc;

  if (!me) return -1;

  if (pid > 0) {
    if (pid_lookup(pid).get() == NULL) {
      return -ECHILD;
    }
  }

  if (pid < -1) {
    KWARN("waitpid(<-1) not implemented\n");
    return -1;
  }

  int result = -EINVAL;


  for (int loops = 0; 1; loops++) {
    struct zombie_entry *zomb = NULL;
    process::ptr found_process = nullptr;


    auto f = waitlist_lock.lock_irqsave();

    bool found = false;
    list_for_each_entry(zomb, &zombie_list, node) {
      if (can_reap(curthd, zomb->proc, pid, wait_flags)) {
        // simply remove the zombie from the zombie_list
        zomb->node.del_init();
        found = true;
        break;
      }
    }
    if (!found) {
      zomb = NULL;
    }


    if (zomb != NULL) {
      found_process = zomb->proc;
      zomb->proc = nullptr;
      // zomb->node.init();
    }

    if (found_process.is_null()) {
      if (wait_flags & WNOHANG) {
        result = -1;
        waitlist_lock.unlock_irqrestore(f);
        break;
      }

      struct wait_entry ent;

      struct waiter_entry went;
      went.thd = curthd;
      went.wait_flags = wait_flags;
      went.seeking = pid;
      went.node.init();
      went.proc = nullptr;

      prepare_to_wait(went.wq, ent);
      waiter_list.add(&went.node);

      waitlist_lock.unlock_irqrestore(f);


      auto sres = ent.start();
      if (sres.interrupted()) {
        result = -EINTR;
        break;
      }

      if (!went.proc) {
        continue;
      }

      found_process = went.proc;
    } else {
      waitlist_lock.unlock_irqrestore(f);
    }

    if (zomb != NULL) {
      assert(zomb->node.is_empty());
      assert(zomb->proc.is_null());
      delete zomb;
    }

    long pid = found_process->pid;
    result = sched::proc::reap(found_process);
    break;
  }


  return result;
}




void sys::exit_thread(int code) {
  // if we are the main thread, exit the group instead.
  if (curthd->pid == curthd->tid) {
    sys::exit_proc(code);
    return;
  }

  // pprintk("TODO: sys::exit_thread!\n");

  // defer to later!
  curthd->should_die = 1;


  sched::exit();
}


static void zombify(struct zombie_entry *zomb) {
  bool f = waitlist_lock.lock_irqsave();

  // first, go over the list of waiters and look for a candidate
  struct waiter_entry *went = NULL;
  list_for_each_entry(went, &waiter_list, node) {
    // if the waiter can reap, remove them from the list, fill in the zent field, memory sync, and
    // wake them up
    if (can_reap(went->thd, zomb->proc, went->seeking, went->wait_flags)) {
      went->node.del_init();
      went->proc = zomb->proc;
      // memory barrier. The above ought to be atomic, really
      __sync_synchronize();
      // release the lock so we can wake up the waiter
      waitlist_lock.unlock_irqrestore(f);


      // free the zombie_entry here, now that we have no locks held
      delete zomb;

      // wake up the waiter
      went->wq.wake_up_all();

      return;
    }
  }


  // ... and add the zombie entry to the list. We didn't find any waiter >_<
  zombie_list.add_tail(&zomb->node);
  waitlist_lock.unlock_irqrestore(f);
}

void sys::exit_proc(int code) {
  if (curproc->pid == 1) panic("INIT DIED!\n");

  {
    scoped_lock l(curproc->datalock);
    for (auto t : curproc->threads) {
      if (t && t != curthd) {
        t->should_die = 1;
        sched::unblock(*t, true);
      }
    }

    while (1) {
      bool everyone_dead = true;
      /* all the threads should be rudely awoken now... */
      for (auto t : curproc->threads) {
        if (t && t != curthd) {
          if (t->get_state() != PS_ZOMBIE) {
            everyone_dead = false;
          }
        }
      }

      if (everyone_dead) break;
      sched::yield();
    }
  }

  curproc->exit_code = code;
  curproc->exited = true;

  curthd->set_state(PS_ZOMBIE);

  // send a signal that we died to the parent process
  // sched::proc::send_signal(curproc->parent->pid, SIGCHLD);

  // to be freed in zombify OR waitpid
  struct zombie_entry *zomb = new zombie_entry(curproc);
  zombify(zomb);

  sched::exit();
}


wait_queue &process::futex_queue(int *uaddr) {
  futex_lock.lock();
  if (!m_futex_queues.contains((off_t)uaddr)) {
    m_futex_queues.set((off_t)uaddr, ck::make_box<wait_queue>());
  }
  auto &wq = *m_futex_queues.ensure((off_t)uaddr);
  futex_lock.unlock();
  return wq;
}




int sys::futex(int *uaddr, int op, int val, int val2, int *uaddr2, int val3) {
  /* If the user can't read the address, it's invalid. */
  if (!VALIDATE_RD(uaddr, 4)) return -EINVAL;

  auto &mm = *curproc->mm;

  off_t addr = (off_t)uaddr;
  /* the address must be word aligned (4 bytes) */
  if ((addr & 0x3) != 0) return -EINVAL;

  /* This follows the general idea of Linux's:
     This operation tests that the value at the futex word pointed to by the address uaddr still
     contains the expected value val, and if so, then sleeps waiting for  a  FU‐ TEX_WAKE  operation
     on  the futex word.  The load of the value of the futex word is an atomic memory access (i.e.,
     using atomic machine instructions of the respective architecture).  This load, the comparison
     with the expected value, and starting to sleep are performed atomically and totally ordered
     with respect to other futex oper‐ ations  on  the  same  futex  word.  If the thread starts to
     sleep, it is considered a waiter on this futex word.  If the futex value does not match val,
     then the call fails immediately with the error EAGAIN.
                                                          */
  if (op == FUTEX_WAIT) {
    auto &wq = curproc->futex_queue(uaddr);
    // printk("[%2d] FUTEX_WAIT - wq: %p\n", curthd->tid, &wq);
    /* Load the item atomically. (ATOMIC ACQUIRE makes sense here I think) */
    int current_value = __atomic_load_n(uaddr, __ATOMIC_ACQUIRE);
    /* If the value is not the expected value, return EAGAIN */
    if (current_value != val) return -EAGAIN;

    // printk("WAIT BEGIN!\n");
    // printk("wait at task list %p\n", &wq.task_list);

    if (wq.wait_exclusive().interrupted()) {
      // printk("FUTEX WAIT WAKEUP (RUDE)\n");
      return -EINTR;
    }
    // printk("FUTEX WAIT WAKEUP\n");

    return 0;
  }

  if (op == FUTEX_WAKE) {
    auto &wq = curproc->futex_queue(uaddr);

    if (val == 0) return 0;

    if (wq.task_list.is_empty_careful()) {
      // printk("task list empty!\n");
      return 0;
    }
    // printk("[%2d] FUTEX_WAKE - wq: %p\n", curthd->tid, &wq);

    wq.__wake_up(0, val, NULL);
    return 1; /* if someone was woken up, return one */
  }

  if (op == FUTEX_DSTR) {
  }

  return -EINVAL;
}



int sys::kill(int pid, int sig) { return sched::proc::send_signal(pid, sig); }

int sys::prctl(int option, unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5) { return -ENOTIMPL; }

#ifdef CONFIG_RISCV
extern "C" void rv_enter_userspace(rv::regs *sp);

static void rv_fork_return(void) {
  auto thd = curthd;
  cpu::switch_vm(thd);
  auto tf = thd->trap_frame;
  auto *regs = (rv::regs *)tf;
  /* return value is zero */
  regs->a0 = 0;

  regs->status = read_csr(sstatus) & ~SSTATUS_SPP;

  arch_enable_ints();
  // printk(KERN_INFO "entering pid %d\n", thd->pid);
  rv_enter_userspace(regs);
}
#endif


static void fork_return(void) {
#ifdef CONFIG_RISCV
  rv_fork_return();
#endif
  return;
}

#ifdef CONFIG_RISCV
extern "C" void trapreturn(void);
#endif

static long do_fork(struct process &p) {
  auto np = sched::proc::spawn_process(&p, SPAWN_FORK);

  /* TLB Flush */
  // arch_flush_mmu();
  int new_pid = np->pid;

  p.children.push(np);

  auto old_td = curthd;
  auto new_td = new thread(np->pid, *np);

  // copy the trapframe
  memcpy(new_td->trap_frame, old_td->trap_frame, arch_trapframe_size());

#ifdef CONFIG_X86
  new_td->trap_frame[0] = 0;  // return value for child is 0
#endif

  // copy floating point
  memcpy(new_td->fpu.state, old_td->fpu.state, 4096);
  new_td->fpu.initialized = old_td->fpu.initialized;

  // go to the fork_return function instead of whatever it was gonna do otherwise
  new_td->kern_context->pc = (u64)fork_return;

  new_td->set_state(PS_RUNNING);
  sched::add_task(new_td);

  // return the child pid to the parent
  return new_pid;
}


int sys::fork(void) {
  int pid = do_fork(*curproc);
  // sched::yield();
  return pid;
}


int sys::sigwait() {
  // :^) this is a bit of a hack. Just allocate a waitqueue on
  // the stack and wait on it (allowing signals)
  wait_queue wq;
  wait_entry ent;

  ent.flags = 0;
  ent.wq = &wq;

  ent.wq->lock.lock();

  sched::set_state(PS_INTERRUPTIBLE);
  ent.wq->task_list.add(&ent.item);

  ent.wq->lock.unlock();


  ent.start();

  /* TODO: Mask? Return which signal was handled? */
  return 0;
}


void sched::proc::dump_table(void) {
  pprintk("Process States: \n");
  for (auto &p : proc_table) {
    auto &proc = p.value;

    for (auto t : proc->threads) {
      const char *state = "UNKNOWN";
#define ST(name) \
  if (t->state == PS_##name) state = #name
      ST(EMBRYO);
      ST(RUNNING);
      ST(INTERRUPTIBLE);
      ST(UNINTERRUPTIBLE);
      ST(ZOMBIE);
#undef ST
      pprintk("  '%s' p:%d,t:%d: %s, %llu\n", proc->name.get(), proc->pid, t->tid, state, proc->mm->memory_usage());
    }
  }


  scoped_irqlock l(waitlist_lock);
  pprintk("Zombie list: \n");
  struct zombie_entry *zent = NULL;
  list_for_each_entry(zent, &zombie_list, node) { pprintk("  process %d\n", zent->proc->pid); }


  thread::dump();
  // pprintk("Waiter List: \n");
  // struct waiter_entry *went = NULL;
  // list_for_each_entry(went, &waiter_list, node) {
  //   pprintk("  process %d, seeking %d\n", went->thd->tid, went->seeking);
  // }
}
