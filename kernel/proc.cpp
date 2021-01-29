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

// start out at pid 2, so init is pid 1 regardless of if kernel threads are
// created before init is spawned
pid_t next_pid = 2;
static spinlock pid_spinlock;
static rwlock ptable_lock;
static map<pid_t, process::ptr> proc_table;

pid_t get_next_pid(void) {
  pid_spinlock.lock();
  auto p = next_pid++;
  pid_spinlock.unlock();
  return p;
}

mm::space *alloc_user_vm(void) {
  return new mm::space(0x1000, 0x7ff000000000, mm::pagetable::create());
}

static process::ptr pid_lookup(pid_t pid) {
  ptable_lock.read_lock();
  process::ptr p = nullptr;
  if (proc_table.contains(pid)) p = proc_table.get(pid);

  ptable_lock.read_unlock();
  return p;
}



void sched::proc::in_pgrp(pid_t pgid, func<bool(struct process &)> cb) {
  ptable_lock.read_lock();

  for (auto kv : proc_table) {
    auto proc = kv.value;
    if (proc->pgid == pgid) {
      if (cb(*proc) == false) break;
    }
  }
  ptable_lock.read_unlock();
}

static process::ptr do_spawn_proc(process::ptr proc_ptr, int flags) {
  // get a reference, they are nicer to look at.
  // (and we spend less time in ref<process>::{ref,deref}())
  auto &proc = *proc_ptr;


  scoped_lock lck(proc.datalock);

  proc.create_tick = cpu::get_ticks();
  proc.embryonic = true;

  proc.ring = flags & SPAWN_KERN ? RING_KERN : RING_USER;



  // This check is needed because the kernel syscall.has no parent.
  if (proc.parent) {
    proc.user = proc.parent->user;  // inherit the user information
    proc.ppid = proc.parent->pid;
    proc.pgid = proc.parent->pgid;  // inherit the group id of the parent

    proc.root = geti(proc.parent->root);
    proc.cwd = geti(proc.parent->cwd);


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

/**
 * Internally, there is no "process table" that keeps track of processes. They
 * are only accessed (and owned) by the tasks that are a member of them. Very
 * democratic. This assures that the lifetime of a process is no longer than
 * that of the constituent tasks.
 */
process::ptr sched::proc::spawn_process(struct process *parent, int flags) {
  // grab the next pid (will be the tid of the main thread when it is executed)
  pid_t pid = get_next_pid();

  // allocate the process
  ptable_lock.write_lock();
  auto proc_ptr = make_ref<process>();
  proc_table.set(pid, proc_ptr);
  ptable_lock.write_unlock();
  if (proc_ptr.get() == NULL) return nullptr;

  proc_ptr->pid = pid;
  proc_ptr->pgid = pid;  // pgid == pid (if no parent)

  // Set up initial data for the process.
  proc_ptr->parent = parent;
  return do_spawn_proc(proc_ptr, flags);
};

bool sched::proc::ptable_remove(pid_t p) {
  bool succ = false;

  ptable_lock.write_lock();
  if (proc_table.contains(p)) {
    proc_table.remove(p);
    succ = true;
  }
  ptable_lock.write_unlock();

  return succ;
}

pid_t sched::proc::spawn_init(vec<string> &paths) {
  pid_t pid = 1;

  process::ptr proc_ptr;

  {
    ptable_lock.write_lock();
    proc_ptr = make_ref<process>();
    if (proc_ptr.get() == NULL) {
      ptable_lock.write_unlock();
      return -1;
    }

    proc_ptr->pid = pid;
    proc_ptr->pgid = pid;  // pgid == pid (if no parent)

    assert(!proc_table.contains(pid));
    proc_table.set(pid, proc_ptr);
    ptable_lock.write_unlock();
  }

  proc_ptr->parent = sched::proc::kproc();

  // initialize normally
  proc_ptr = do_spawn_proc(proc_ptr, 0);

  // init starts in the root directory
  proc_ptr->root = geti(vfs::get_root());
  proc_ptr->cwd = geti(vfs::get_root());
  auto &proc = *proc_ptr;

  vec<string> envp;
  for (auto &path : paths) {
    vec<string> argv;
    argv.push(path);

    int res = proc.exec(path, argv, envp);
		printk("exec '%s' res: %d\n", path.get(), res);
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
    kernel_process->embryonic = false;
    // auto &vm = kernel_process->mm;
    delete kernel_process->mm;
    kernel_process->mm = &mm::space::kernel_space();
  }

  return kernel_process.get();
}


struct thread *sched::proc::spawn_kthread(const char *name, int (*func)(void *), void *arg) {
  auto proc = kproc();
  auto tid = get_next_pid();
  auto thd = new thread(tid, *proc);
  thd->trap_frame[1] = (unsigned long)arg;


  arch_reg(REG_PC, thd->trap_frame) = (unsigned long)func;
  thd->state = PS_RUNNING;

  thd->setup_tls();



  return thd;
}


pid_t sched::proc::create_kthread(const char *name, int (*func)(void *), void *arg) {
  auto proc = kproc();

  auto tid = get_next_pid();

  // construct the thread
  auto thd = new thread(tid, *proc);
  thd->trap_frame[1] = (unsigned long)arg;

  thd->kickoff((void *)func, PS_RUNNING);

  KINFO("Created kernel thread '%s'. tid=%d\n", name, tid);

  return tid;
}

ref<fs::file> process::get_fd(int fd) {
  ref<fs::file> file;

  scoped_lock l(file_lock);
  auto it = open_files.find(fd);
  if (it != open_files.end()) {
    file = it->value;
  }

  return file;
}

int process::add_fd(ref<fs::file> file) {
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

void sched::proc::dump_table(void) {
  printk("process dump:\n");

  printk("   ticks: %zu\n", cpu::get_ticks());
  printk("   sched: rip=%p\n", cpu::current().sched_ctx->pc);
  for (auto &p : proc_table) {
    auto &proc = p.value;
    printk("pid %d ", proc->pid);
    printk("threadc=%-3d ", proc->threads.size());

    printk("dead= %d ", proc->is_dead());

    printk("files={");

    int ifd = 0;
    for (auto &fd : proc->open_files) {
      printk("%d", fd.key);

      ifd++;
      if (ifd <= proc->open_files.size() - 1) {
        printk(", ");
      }
    }
    printk("} ");
    printk("embryo=%d ", proc->embryonic);

    printk("\n");

    for (auto tid : proc->threads) {
      auto t = thread::lookup(tid);
      printk("tid %-3d ", tid);
      printk("ticks %-4d ", t->sched.ticks);
      const char *state = "UNKNOWN";
#define ST(name) \
  if (t->state == PS_##name) state = #name
      ST(EMBRYO);
      ST(RUNNING);
      ST(INTERRUPTIBLE);
      ST(UNINTERRUPTIBLE);
      ST(ZOMBIE);
#undef ST

      // printk("name %-10s ", t->);
      printk("state %-10s ", state);
      printk("rip %p ", t->trap_frame != NULL ? arch_reg(REG_PC, t->trap_frame) : 0);

      printk("pri %-3d ", t->sched.priority);
      printk("die %-3d ", t->should_die);
      printk("SC %-7d ", t->stats.syscall_count);
      printk("RC %-7d ", t->stats.run_count);
      printk("timeslice %-3d ", t->sched.timeslice);
      printk("start %-6d ", t->sched.start_tick);
      printk("cpu %-3d ", t->stats.current_cpu);


      printk("\n");
    }

    // proc->mm->dump();
    printk("\n");
  }
}

bool process::is_dead(void) {
  for (auto tid : threads) {
    auto t = thread::lookup(tid);
    assert(t);
    scoped_lock l(t->locks.generic);

    if (t->state != PS_ZOMBIE) {
      return false;
    }
  }

  return true;
}

int process::exec(string &path, vec<string> &argv, vec<string> &envp) {
  scoped_lock lck(this->datalock);


  // try to load the binary
  fs::inode *exe = NULL;

  // TODO: open permissions on the binary
  if (vfs::namei(path.get(), 0, 0, cwd, exe) != 0) {
    return -ENOENT;
  }
  // TODO check execution permissions

  off_t entry = 0;
  auto fd = make_ref<fs::file>(exe, FDIR_READ);

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

  //
  this->embryonic = false;

  struct thread *thd;
  if (threads.size() != 0) {
    thd = thread::lookup(this->pid);
    assert(thd != NULL);
  } else {
    thd = new thread(pid, *this);
  }

  // construct the thread
  arch_reg(REG_SP, thd->trap_frame) = stack + stack_size - 64;
  thd->kickoff((void *)entry, PS_RUNNING);

  return 0;
}

int sched::proc::send_signal(pid_t p, int sig) {
  // TODO: handle process group signals
  if (p < 0) {
    return -ENOTIMPL;
  }

  if (sig < 0 || sig >= 64) return -EINVAL;
  ptable_lock.read_lock();

  int err = -ESRCH;

  if (proc_table.contains(p)) {
    auto &targ = proc_table[p];


    // find a thread
    for (auto &tid : targ->threads) {
      auto *thd = thread::lookup(tid);
      assert(thd != NULL);
      if (thd->send_signal(sig)) {
        err = 0;
        break;
      }
    }
  }

  ptable_lock.read_unlock();

  return err;
}

int sched::proc::reap(process::ptr p) {
  assert(p->is_dead());
  auto *me = curproc;


  int f = 0;
  f |= (p->exit_code & 0xFF) << 8;

#ifdef CONFIG_VERBOSE_PROCESS
  printk("reap (p:%d) on cpu %d\n", p->pid, cpu::current().cpunum);
  auto usage = p->mm->memory_usage();
  printk("  ram usage: %zu Kb (%zu b)\n", usage / KB, usage);
#endif

  process::ptr init = proc_table[1];
  assert(init);

  for (auto tid : p->threads) {
    auto *t = thread::lookup(tid);
    assert(t->state == PS_ZOMBIE);
#ifdef CONFIG_VERBOSE_PROCESS
    printk("  [t:%d] sc:%d rc:%d\n", t->tid, t->stats.syscall_count, t->stats.run_count);
#endif

    /* make sure... */
    t->locks.run.lock();
    thread::teardown(t);
  }
  p->threads.clear();
  /* remove the child from our list */
  for (int i = 0; i < me->children.size(); i++) {
    if (me->children[i] == p) {
      me->children.remove(i);
      break;
    }
  }

  // release the CWD and root
  fs::inode::release(p->cwd);
  p->cwd = NULL;

  fs::inode::release(p->root);
  p->root = NULL;

  if (me != init) init->datalock.lock();
  for (auto &c : p->children) {
    printk("pid %d adopted by init\n", c->pid);
    init->children.push(c);
    c->parent = init;
  }
  if (me != init) init->datalock.unlock();

  ptable_remove(p->pid);

  return f;
}




static spinlock dumb_waitpid_lock;

void sys::exit_thread(int code) {
  // if we are the main thread, exit the group instead.
  if (curthd->pid == curthd->tid) {
    sys::exit_proc(code);
    return;
  }
  // defer to later!
  curthd->should_die = 1;

  panic("TODO: sys::exit_thread!\n");

  sched::exit();
}

void sys::exit_proc(int code) {
  if (curproc->pid == 1) panic("INIT DIED!\n");


  {
    scoped_lock l(curproc->datalock);
    for (auto tid : curproc->threads) {
      auto t = thread::lookup(tid);
      if (t && t != curthd) {
        t->should_die = 1;
        sched::unblock(*t, true);
      }
    }

    /* all the threads should be rudely awoken now... */
    for (auto tid : curproc->threads) {
      auto t = thread::lookup(tid);
      if (t && t != curthd) {
        // take the run lock for now... This ensures that the thread has completed.
        t->locks.run.lock();
        assert(t->state != PS_ZOMBIE);
        t->locks.run.unlock();
      }
    }
  }



  dumb_waitpid_lock.lock();
  curproc->exit_code = code;
  curproc->exited = true;

  sched::proc::send_signal(curproc->parent->pid, SIGCHLD);

  curproc->parent->datalock.lock();
  curproc->child_wq.wake_up_all();

  // curproc->child_wq.wake_up_common(0, 1, 0, (void *)curproc);
  curproc->parent->datalock.unlock();

  dumb_waitpid_lock.unlock();

  sched::exit();
}


/* The state  */
struct wait_object {
  int req;     /* first argument in waitpid */
  int options; /* Third argument... duh :) */

  /* The process we found :) */
  struct process *proc;
};



static bool waitpid_consider(struct wait_object &wo, struct process *proc) {
  bool accept = false;

  if (!proc->is_dead()) return false;
  if (wo.req == -1) return true;
  if (wo.req == proc->pid) return true;

  return false;
}

/* curproc->datalock is held by the waker */
static bool waitpid_wake_function(struct wait_entry *entry, unsigned int mode, int sync, void *key) {
  /* Key is the proc that wakes us up */
  struct process *proc = (struct process *)key;

  auto *wo = entry->priv<wait_object>();
  printk("wake up with %p\n", key);

  if (waitpid_consider(*wo, proc)) {
    /* enter the  */
    wo->proc = proc;
    return autoremove_wake_function(entry, mode, sync, key);
  } else {
    /* don't wake us up */
    return false;
  }
}


int wait_check(struct process *me, int pid /* 1st arg to waitpid */, int &status, int options) {
  scoped_lock l(me->datalock);
  process::ptr targ = nullptr;
  if (pid == -1) {
    for (auto c : me->children) {
      if (c) {
        if (c->is_dead()) {
          status = sched::proc::reap(c);
          return c->pid;
        }
      }
    }
  } else if (pid != -1) {
    targ = pid_lookup(pid);
    if (!targ) return -1;
    if (targ->parent != me) {
      return -ECHILD;
    }
  }

  if (targ) {
    if (targ->is_dead()) {
      status = sched::proc::reap(targ);
      return targ->pid;
    }
  }
  return 0;
}

int sched::proc::do_waitpid(pid_t pid, int &status, int options) {
  auto *me = curproc;

  if (!me) return -1;

  if (pid != -1) {
    if (pid_lookup(pid).get() == NULL) {
      return -ECHILD;
    }
  }

  if (pid < -1) {
    KWARN("waitpid(<-1) not implemented\n");
    return -1;
  }


#ifdef CONFIG_VERBOSE_PROCESS
  printk(KERN_DEBUG "Process %d is waiting for pid=%d, options=", me->pid, pid);
  if (options & WNOHANG) printk("NOHANG ");
  if (options & WUNTRACED) printk("WUNTRACED ");
  printk("\n");
#endif

  /* Look through existing proces for dead ones. Maybe we don't have to wait? */

  int result = -EINVAL;

  while (1) {
    /*
     * "Allocate" one of these on each go around on this, so
     * it gets destructed when breaking or continuing
     */
    struct wait_entry ent;

    dumb_waitpid_lock.lock();

    prepare_to_wait_exclusive(me->child_wq, ent);

    result = wait_check(me, pid, status, options);

    dumb_waitpid_lock.unlock();
    if (result != 0) break;
    if (options & WNOHANG) {
      result = -1;
      break;
    }

    sched::yield();
  }
  // finish_wait(me->child_wq, ent);

  return result;
}


wait_queue &process::futex_queue(int *uaddr) {
  futex_lock.lock();
  if (!m_futex_queues.contains((off_t)uaddr)) {
    m_futex_queues.set((off_t)uaddr, make_unique<wait_queue>());
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
     This operation tests that the value at the futex word pointed to by the address uaddr still contains the expected
     value val, and if so, then sleeps waiting for  a  FU‐ TEX_WAKE  operation  on  the futex word.  The load of the
     value of the futex word is an atomic memory access (i.e., using atomic machine instructions of the respective
                architecture).  This load, the comparison with the expected value, and starting to sleep are performed
     atomically and totally ordered with respect to other futex oper‐ ations  on  the  same  futex  word.  If the thread
     starts to sleep, it is considered a waiter on this futex word.  If the futex value does not match val, then the
     call fails immediately with the error EAGAIN.
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

int sys::prctl(int option, unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5) {
  return -ENOTIMPL;
}



static void trap_return(void) { return; }

static pid_t do_fork(struct process &p) {
  auto np = sched::proc::spawn_process(&p, SPAWN_FORK);
  sched::yield();

  /* TLB Flush */
  arch_flush_mmu();
  int new_pid = np->pid;

  np->embryonic = false;
  p.children.push(np);

  auto old_td = curthd;
  auto new_td = new thread(np->pid, *np);

  // copy the trapframe
  memcpy(new_td->trap_frame, old_td->trap_frame, arch_trapframe_size());
  new_td->trap_frame[0] = 0;  // return value for child is 0


  // copy floating point
  memcpy(new_td->fpu.state, old_td->fpu.state, 4096);
  new_td->fpu.initialized = old_td->fpu.initialized;

  // go to the trap_return function instead of whatever it was gonna do otherwise
  new_td->kern_context->pc = (u64)trap_return;

  new_td->state = PS_RUNNING;
  sched::add_task(new_td);


  // return the child pid to the parent
  return new_pid;
}


int sys::fork(void) { return do_fork(*curproc); }
