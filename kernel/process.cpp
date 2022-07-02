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
#include <chan.h>
#include <lock.h>
#include <mem.h>
#include <phys.h>
#include <sched.h>
#include <syscall.h>
#include <util.h>
#include <wait_flags.h>
#include <module.h>
#include "asm.h"

#ifdef CONFIG_RISCV
#include <riscv/arch.h>
#endif

// start out at pid 2, so init is pid 1 regardless of if kernel threads are
// created before init is spawned
volatile long next_pid = 2;

// static rwlock ptable_lock;
static spinlock ptable_lock;
static ck::map<long, ck::ref<Process>> proc_table;

long get_next_pid(void) { return __atomic_add_fetch(&next_pid, 1, __ATOMIC_ACQUIRE); }

mm::AddressSpace *alloc_user_vm(void) {
  unsigned long top = 0x7ff000000000;
#ifdef CONFIG_SV39
  top = 0x3ff0000000;
#endif

  top -= rand() & 0xFFFFF000;
  return new mm::AddressSpace(0x1000, top, mm::PageTable::create());
}

ck::ref<Process> pid_lookup(long pid) {
  scoped_irqlock l(ptable_lock);
  ck::ref<Process> p = nullptr;
  if (proc_table.contains(pid)) {
    p = proc_table.get(pid);
  }

  return p;
}



void sched::proc::in_pgrp(long pgid, ck::func<bool(struct Process &)> cb) {
  scoped_irqlock l(ptable_lock);
  for (auto kv : proc_table) {
    auto proc = kv.value;
    if (proc->pgid == pgid) {
      if (cb(*proc) == false) break;
    }
  }
}

static ck::ref<Process> do_spawn_proc(ck::ref<Process> proc_ptr, int flags) {
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

    proc.root = proc.parent->root;
    proc.cwd = proc.parent->cwd;

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

  if (proc.cwd == nullptr) {
    proc.cwd = vfs::get_root();
  }


  if (flags & SPAWN_FORK) {
    proc.mm = proc.parent->mm->fork();
  } else {
    proc.mm = alloc_user_vm();
  }
  return proc_ptr;
}

ck::ref<Process> sched::proc::spawn_process(struct Process *parent, int flags) {
  // grab the next pid (will be the tid of the main thread when it is executed)
  long pid = get_next_pid();


  ck::ref<Process> proc;
  proc = ck::mk<Process>();
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

  ck::ref<Process> proc_ptr;
  proc_ptr = ck::make_ref<Process>();
  if (proc_ptr.get() == NULL) {
    printf("Failed to allocate process\n");
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
  proc_ptr->root = curproc->root;
  proc_ptr->cwd = curproc->root;
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



static ck::ref<Process> kernel_process = nullptr;


Process *sched::proc::kproc(void) {
  if (kernel_process.get() == nullptr) {
    // spawn the kernel process
    kernel_process = sched::proc::spawn_process(nullptr, SPAWN_KERN);
    kernel_process->pid = 0;  // just lower than init (pid 1)
    kernel_process->name = "kernel";
    delete kernel_process->mm;
    kernel_process->mm = &mm::AddressSpace::kernel_space();
  }

  return kernel_process.get();
}


ck::ref<Thread> sched::proc::spawn_kthread(const char *name, int (*func)(void *), void *arg) {
  auto proc = kproc();
  auto tid = get_next_pid();
  auto thd = ck::make_ref<Thread>(tid, *proc);
  thd->trap_frame[1] = (unsigned long)arg;
  thd->name = name;


  arch_reg(REG_PC, thd->trap_frame) = (unsigned long)func;
  thd->set_state(PS_RUNNING);

  return thd;
}


long sched::proc::create_kthread(const char *name, int (*func)(void *), void *arg) {
  auto proc = kproc();

  auto tid = get_next_pid();

  // construct the thread
  auto thd = ck::make_ref<Thread>(tid, *proc);
  thd->trap_frame[1] = (unsigned long)arg;
  thd->name = name;

  thd->kickoff((void *)func, PS_RUNNING);

  KINFO("Created kernel thread '%s'. tid=%d\n", name, tid);

  return tid;
}

ck::ref<fs::File> Process::get_fd(int fd) {
  ck::ref<fs::File> file;

  scoped_lock l(file_lock);
  auto it = open_files.find(fd);
  if (it != open_files.end()) {
    file = it->value;
  }

  return file;
}

int Process::add_fd(ck::ref<fs::File> file) {
  int fd = 0;
  scoped_lock l(file_lock);

  while (1) {
    if (!open_files.contains(fd)) break;
    fd++;
  }

  open_files.set(fd, file);
  return fd;
}

Process::~Process(void) {
  if (threads.size() != 0) {
    KWARN("destruction of proc %d still has threads!\n", this->pid);
  }

  sched::proc::ptable_remove(this->pid);
  delete mm;
}

bool Process::is_dead(void) {
  for (auto t : threads) {
    assert(t);

    if (t->get_state() != PS_ZOMBIE) {
      return false;
    }
  }

  return true;
}


int Process::exec(ck::string &path, ck::vec<ck::string> &argv, ck::vec<ck::string> &envp) {
  scoped_lock lck(this->datalock);


  // try to load the binary
  ck::ref<fs::Node> exe = nullptr;

  // TODO: open permissions on the binary
  if (vfs::namei(path.get(), 0, 0, cwd, exe) != 0) {
    printf("File, '%s', does not exist\n", path.get());
    return -ENOENT;
  }
  // TODO check execution permissions

  off_t entry = 0;
  auto fd = ck::make_ref<fs::File>(exe, FDIR_READ);

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

  ck::ref<Thread> thd;
  if (threads.size() != 0) {
    thd = Thread::lookup(this->pid);
  } else {
    thd = ck::make_ref<Thread>(pid, *this);
  }


  // construct the thread
  arch_reg(REG_SP, thd->trap_frame) = stack + stack_size - 64;
  thd->kickoff((void *)entry, PS_RUNNING);

  return 0;
}



int sched::proc::reap(ck::ref<Process> p) {
  // assert(p->is_dead());
  auto *me = curproc;

  int f = 0;

  f |= (p->exit_signal & 0xFF);
  f |= (p->exit_code & 0xFF) << 8;

#ifdef CONFIG_VERBOSE_PROCESS
  pprintf("reap (p:%d, pg:%d) on cpu %d\n", p->pid, p->pgid, cpu::current().id);
  auto usage = p->mm->memory_usage();
  pprintf("  ram usage: %zu Kb (%zu b)\n", usage / KB, usage);
#endif

  assert(p->threads.size() == 0);

  /* remove the child from our list */
  {
    scoped_irqlock l(me->datalock);
    for (int i = 0; i < me->children.size(); i++) {
      if (me->children[i] == p) {
        me->children.remove(i);
        break;
      }
    }
  }

  // release the CWD and root
  p->cwd = nullptr;
  p->root = nullptr;

  // If the process had children, we need to give them to init
  if (p->children.size() != 0) {
    // grab a pointer to init
    ck::ref<Process> init = pid_lookup(1);
    assert(init);
    init->datalock.lock();
    for (auto &c : p->children) {
      c->datalock.lock();
      init->children.push(c);
      c->parent = init;
      c->datalock.unlock();
    }
    init->datalock.unlock();
  }

  ptable_remove(p->pid);

  return f;
}


void Process::terminate(int signal) {
  // pprintf("Terminate with signal %d\n", signal);
  signal &= 0x7F;
  /* top bit of the first byte means it was signalled. Everything else is the signal that killed it
   */
  this->exit_signal = signal | 0x80;
  this->exit(signal + 128);
  // sys::exit_proc(signal + 128);
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
  ck::ref<Process> proc;
  zombie_entry(ck::ref<Process> proc) : proc(proc) { node.init(); }
  ~zombie_entry(void) {}
};

struct waiter_entry {
  // an entry in the waiter_list
  struct list_head node;
  long seeking;  // the pid that this waiter is interested in (waitpid systemcall semantics)
  // systemcall wait flags
  int wait_flags = 0;
  // the process that we caught
  ck::ref<Process> proc;
  // the waiting thread
  ck::ref<Thread> thd;
  // the waitqueue that the process sits on
  wait_queue wq;
};




static bool can_reap(Thread *reaper, ck::ref<Process> zombie, long seeking, int reap_flags) {
  if (!zombie->parent) return false;
  // a process may not reap another process's children.
  if (zombie->parent->pid != reaper->proc.pid) return false;

  if (zombie->pid == seeking) return true;

  // if you seek -1, and the process is your child, you can reap it
  if (seeking == -1) return true;

  // if the pid is less than -1, you are actually waiting on a process group
  if (seeking < -1 && zombie->pgid == -seeking) return true;

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


  auto self = curthd;
  for (int loops = 0; 1; loops++) {
    struct zombie_entry *zomb = NULL;
    ck::ref<Process> found_process = nullptr;

    auto f = waitlist_lock.lock_irqsave();

    bool found = false;
    list_for_each_entry(zomb, &zombie_list, node) {
      if (can_reap(self, zomb->proc, pid, wait_flags)) {
        // simply remove the zombie from the zombie_list
        zomb->node.del_init();
        found = true;
        break;
      }
    }

    if (!found) zomb = NULL;

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
      went.thd = self;
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




static void zombify(struct zombie_entry *zomb) {
  scoped_irqlock l(waitlist_lock);


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

      // wake up the waiter
      went->wq.wake_up_all();

      // release the lock so we can wake up the waiter

      // free the zombie_entry here, now that we have no locks held
      delete zomb;
      return;
    }
  }

  // ... and add the zombie entry to the list. We didn't find any waiter >_<
  zombie_list.add_tail(&zomb->node);
}




static chan<long /* pid */> to_reap;
/**
 * Process reaper task.
 *
 * Listens on a pipe for dead processes and frees up their resources.
 */
int Process::reaper(void *) {
  while (1) {
    auto pid = to_reap.recv();

    ck::ref<Process> p = ::pid_lookup(pid);

    if (!p) panic("reaper: could not find pid %d\n", pid);


    // teardown all the threads in the process
    {
      ck::vec<ck::ref<Thread>> to_teardown = p->threads;
      for (auto t : to_teardown) {
        while (1) {
          auto f = t->joinlock.lock_irqsave();
          if (t->get_state() == PS_ZOMBIE) {
            t->joinlock.unlock_irqrestore(f);
            break;
          }
          wait_entry ent;
          prepare_to_wait(t->joiners, ent);

          // t->exit();
          t->joinlock.unlock_irqrestore(f);

          ent.start();
        }

        /* make sure... */

        t->runlock.lock();
        Thread::teardown(move(t));
      }
    }

    assert(p->threads.size() == 0);


    // printf("proc: %d -> %p\n", pid, p.get());
    struct zombie_entry *zomb = new zombie_entry(p);
    zombify(zomb);
  }

  return 0;
}




void sys::exit_proc(int code) { curproc->exit(code); }


void Process::exit(int code) {
  {
    scoped_irqlock l(datalock);
    auto all = threads;
    for (auto t : all) {
      t->exit();
    }
  }

  exit_code = code;
  exited = true;

  to_reap.send(move(this->pid), false);
}

void sys::exit_thread(int code) {
  // if we are the main thread, exit the group instead.
  if (curthd->pid == curthd->tid) {
    return curproc->exit(code);
  }

  curthd->exit();
}



wait_queue &Process::futex_queue(int *uaddr) {
  futex_lock.lock();
  if (!m_futex_queues.contains((off_t)uaddr)) {
    m_futex_queues.set((off_t)uaddr, ck::make_box<wait_queue>());
  }
  auto &wq = *m_futex_queues.ensure((off_t)uaddr);
  futex_lock.unlock();
  return wq;
}




int sys::futex(int *uaddr, int op, int val, int val2, int *uaddr2, int val3) {
  // printf("call futex %p %d\n", uaddr, op);
  /* If the user can't read the address, it's invalid. */
  if (!VALIDATE_RD(uaddr, 4)) return -EINVAL;

  auto &mm = *curproc->mm;

  off_t addr = (off_t)uaddr;
  /* the address must be word aligned (4 bytes) */
  if ((addr & 0x3) != 0) return -EINVAL;

  /*
   * This follows the general idea of Linux's:
   * This operation tests that the value at the futex word pointed to by the address uaddr still
   * contains the expected value val, and if so, then sleeps waiting for  a  FU‐ TEX_WAKE  operation
   * on  the futex word.  The load of the value of the futex word is an atomic memory access (i.e.,
   * using atomic machine instructions of the respective architecture).  This load, the comparison
   * with the expected value, and starting to sleep are performed atomically and totally ordered
   * with respect to other futex oper‐ ations  on  the  same  futex  word.  If the thread starts to
   * sleep, it is considered a waiter on this futex word.  If the futex value does not match val,
   * then the call fails immediately with the error EAGAIN.
   */
  if (op == FUTEX_WAIT) {
    // printf("futex_wait(%p, %d)\n", uaddr, val);
    auto &wq = curproc->futex_queue(uaddr);

    wait_entry ent;
    prepare_to_wait_exclusive(wq, ent, true);
    // prepare_to_wait(wq, ent, true);
    // printf("[%2d] FUTEX_WAIT - wq: %p\n", curthd->tid, &wq);
    /* Load the item atomically. (ATOMIC ACQUIRE makes sense here I think) */
    int current_value = __atomic_load_n(uaddr, __ATOMIC_ACQUIRE);
    /* If the value is not the expected value, return EAGAIN */
    if (current_value != val) return -EAGAIN;

    // printf("WAIT BEGIN!\n");

    if (ent.start().interrupted()) {
      // printf("FUTEX WAIT WAKEUP (RUDE)\n");
      return -EINTR;
    }
    // printf("FUTEX WAIT WAKEUP\n");

    return 0;
  }

  if (op == FUTEX_WAKE) {
    // printf("futex_wake(%p, %d)\n", uaddr, val);
    auto &wq = curproc->futex_queue(uaddr);

    if (val == 0) return 0;

    if (wq.task_list.is_empty_careful()) {
      return 0;
    }
    // printf("[%2d] FUTEX_WAKE - wq: %p\n", curthd->tid, &wq);

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
  // printf(KERN_INFO "entering pid %d\n", thd->pid);
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

static long do_fork(struct Process &p) {
  auto np = sched::proc::spawn_process(&p, SPAWN_FORK);

  /* TLB Flush */
  // arch_flush_mmu();
  int new_pid = np->pid;

  p.children.push(np);

  auto old_td = curthd;
  auto new_td = ck::make_ref<Thread>(np->pid, *np);

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

extern void sched_barrier();

extern ck::map<long, ck::weak_ref<Thread>> thread_table;



size_t utf8_strlen(const char *s) {
  size_t count = 0;
  while (*s)
    count += (*s++ & 0xC0) != 0x80;
  return count;
}


struct TableLogger {
  using Columns = ck::vec<ck::string>;
  using Row = ck::map<ck::string, ck::string>;

  TableLogger(Columns columns) : columns(columns) {}

  void add_row(Row &row) { rows.push(row); }
  void display(void) {
    ck::map<ck::string, int> widths;
    for (auto &col : columns)
      widths[col] = col.len();
    for (auto &row : rows) {
      for (auto &[c, v] : row) {
        widths[c] = max(widths[c], utf8_strlen(v.get()));
      }
    }
    constexpr const char *entry_format = "| %-*s ";
    for (auto &c : columns) {
      printf(entry_format, widths[c], c.get());
    }
    printf("\n");

    ck::string val;
    for (auto &row : rows) {
      for (auto &c : columns) {
        val = "";
        if (row.contains(c)) {
          val = row[c].get();
        }

        int to_add = widths[c] - utf8_strlen(val.get());
        for (int i = 0; i < to_add; i++)
          val += " ";

        printf(entry_format, widths[c], val.get());
      }
      printf("|\n");
    }
  }

 private:
  Columns columns;
  ck::vec<Row> rows;
};

static TableLogger::Row dump_thread(Thread &t) {
  TableLogger::Row r;
  ck::string state = "UNK";
#define ST(name, s) \
  if (t.state == PS_##name) state = s
  ST(RUNNING, "R");
  ST(INTERRUPTIBLE, "S");
  ST(UNINTERRUPTIBLE, "D");
  ST(ZOMBIE, "Z");
#undef ST

  if (t.proc.ring == RING_KERN) state += "k";  // *k*ernel

  r["STAT"] = state;
  r["PID"] = ck::string::format("%d", t.pid);
  r["TID"] = ck::string::format("%d", t.tid);
  r["NAME"] = t.name;


  // printf("%3dp %3dt ", t.pid, t.tid);

  {
    scoped_irqlock l(t.schedlock);
    if (t.scheduler) {
      r["CORE"] = ck::string::format("%d", t.scheduler->core().id);
    }

    r["TIME"] = ck::string::format("%lu.%lums", t.ktime_us / 1000, t.ktime_us % 1000);

    auto c = t.constraint();
    switch (c.type) {
      case rt::APERIODIC:
        r["REALTIME"] = ck::string::format("A μ:%d", c.aperiodic.priority);
        break;
      case rt::PERIODIC:
        r["REALTIME"] = ck::string::format("P φ:%d τ:%d σ:%d", c.periodic.phase, c.periodic.period, c.periodic.slice);
        break;
      case rt::SPORADIC:
        r["REALTIME"] = ck::string::format(
            "S φ:%d ω:%d δ:%d μ:%d", c.sporadic.phase, c.sporadic.size, c.sporadic.deadline, c.sporadic.aperiodic_priority);
        break;
    }
  }
  // printf("  %s\n", t.name.get());
  // printf("p%-3d t%-3d c%2d %3s %s\n", t.pid, t.tid, core, state, t.name.get());
  return r;
}

void dump_process_table_internal(void) {
  // tid -> pid mappings for debug. This allows this routine to
  // print threads that no longer have processes but are still
  // sitting around somewhere.
  ck::map<int, int> associated_threads;
  TableLogger::Columns cols;
  cols.push("PID");
  cols.push("TID");
  cols.push("CORE");
  cols.push("STAT");
  cols.push("REALTIME");
  cols.push("TIME");
  cols.push("NAME");
  TableLogger logger(cols);


  printf("------ Process Dump ------\n");
  for (auto &[pid, proc] : proc_table) {
    for (auto t : proc->threads) {
			if (t->kern_idle) continue;
      associated_threads[t->tid] = pid;
      auto row = dump_thread(*t);
      logger.add_row(row);
    }
  }


	printf("ticks: %llu\n", core().kstat.ticks);
  logger.display();

  // printf("------ Leaked ------\n");
  // for (auto &[tid, thd] : thread_table) {
  //   if (associated_threads[tid] == false) {
  //     auto t = thd.get();
  //     if (t) dump_thread(*t);
  //   }
  // }
  // printf("------ Zombies ------\n");
  // struct zombie_entry *zent = NULL;
  // list_for_each_entry(zent, &zombie_list, node) { printf("  process %d\n", zent->proc->pid); }
  // sched::dump();
}
void sched::proc::dump_table(void) {
  // call everyone (and barrier at the end) to make sure that the scheduler
  // doesn't decide to change this state out from under us on some other core
  cpu::xcall_all(
      [](void *) {
        if (core_id() == 0) {
					dump_process_table_internal();
        }

        // Make sure the printing core is done before letting any core
        // go back to their business.
        sched_barrier();
      },
      NULL);
}


ksh_def("pdump", "dump the process table") {
  sched::proc::dump_table();
  return 0;
}
