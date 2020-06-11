/**
 * This file implements process functions from sched::proc::*
 * in include/sched.h
 */

#include <cpu.h>
#include <elf/loader.h>
#include <errno.h>
#include <fs.h>
#include <fs/vfs.h>
#include <lock.h>
#include <mem.h>
#include <paging.h>
#include <phys.h>
#include <sched.h>
#include <wait_flags.h>
#include <util.h>

// start out at pid 2, so init is pid 1 regardless of if kernel threads are
// created before init is spawned
pid_t next_pid = 2;
static spinlock pid_spinlock;
static rwlock ptable_lock;
static map<pid_t, process::ptr> proc_table;

static pid_t get_next_pid(void) {
  pid_spinlock.lock();
  auto p = next_pid++;
  pid_spinlock.unlock();
  return p;
}

static mm::space *alloc_user_vm(void) {
  return new mm::space(0x1000, 0x7ffffffff000, mm::pagetable::create());
}

static process::ptr pid_lookup(pid_t pid) {
  ptable_lock.read_lock();
  process::ptr p = nullptr;
  if (proc_table.contains(pid)) p = proc_table.get(pid);

  ptable_lock.read_unlock();
  return p;
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
    proc.user = proc_ptr->parent->user;  // inherit the user information
    proc.ppid = proc_ptr->parent->pid;
    proc.pgid = proc_ptr->parent->pgid;  // inherit the group id of the parent

		proc.root = geti(proc.parent->root);
		proc.cwd = geti(proc.parent->cwd);

    // inherit stdin(0) stdout(1) and stderr(2)
    for (int i = 0; i < 3; i++) {
      proc.open_files[i] = proc_ptr->parent->get_fd(i);
    }
  }

  if (proc.cwd == NULL) {
    proc.cwd = vfs::get_root();
  }
  // special case for kernel's init
  if (proc.cwd != NULL) geti(proc.cwd);

  proc.mm = alloc_user_vm();
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

pid_t sched::proc::create_kthread(const char *name, int (*func)(void *),
                                  void *arg) {
  auto proc = kproc();

  auto tid = get_next_pid();

  // construct the thread
  auto thd = new thread(tid, *proc);
  thd->trap_frame[1] = (unsigned long)arg;

  thd->kickoff((void *)func, PS_RUNNABLE);

  KINFO("Created kernel thread '%s'. tid=%d\n", name, tid);

  return tid;
}

pid_t process::fork(void) { return -1; }

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
  printk("   sched: rip=%p\n", cpu::current().sched_ctx->eip);
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
      ST(UNRUNNABLE);
      ST(RUNNABLE);
      ST(ZOMBIE);
      ST(BLOCKED);
      ST(EMBRYO);
#undef ST

      printk("state %-10s ", state);

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

	// auto &st = cpu::current().kstat;
	// printk("t:%-8zu u:%-8zu k:%-8zu i:%-8zu\n", st.ticks, st.uticks, st.kticks, st.iticks);

	/*
	printk("exec (pid=%d) %s:\n", pid, path.get());
	for (int i = 0; i < argv.size(); i++) {
		printk("   argv[%d] = '%s'\n", i, argv[i].get());
	}
	for (int i = 0; i < envp.size(); i++) {
		printk("   envp[%d] = '%s'\n", i, envp[i].get());
	}
	*/


  if (!this->embryonic) return -1;

  if (threads.size() != 0) return -1;

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

  stack = new_addr_space->mmap("[stack]", 0, stack_size, PROT_READ | PROT_WRITE,
                               MAP_ANON | MAP_PRIVATE, nullptr, 0);

  // TODO: push argv and arguments onto the stack
  this->args = argv;
  this->env = envp;
  delete this->mm;
  this->mm = new_addr_space;

  //
  this->embryonic = false;

  // construct the thread
  auto thd = new thread(pid, *this);
  arch::reg(REG_SP, thd->trap_frame) = stack + stack_size - 64;
  thd->kickoff((void *)entry, PS_RUNNABLE);

  return 0;
}

bool sched::proc::send_signal(pid_t p, int sig) {
  if (sig < 0 || sig >= 64) return false;
  ptable_lock.read_lock();

  bool sent = false;

	/*

  if (proc_table.contains(p)) {
    auto &targ = proc_table[p];
    scoped_lock siglock(targ->sig.lock);
    if (!(targ->sig.mask & SIGBIT(sig))) {
      printk("sending signal %d to %d\n", p, sig);
      targ->sig.pending |= SIGBIT(sig);
      sent = true;
    }
  }
	*/

  ptable_lock.read_unlock();

  return sent;
}

// #define REAP_DEBUG

int sched::proc::reap(process::ptr p) {
  assert(p->is_dead());
  auto *me = curproc;


  int f = 0;
  f |= (p->exit_code & 0xFF) << 8;

#ifdef REAP_DEBUG
  printk("reap (p:%d) on cpu %d\n", p->pid, cpu::current().cpunum);
  auto usage = p->mm->memory_usage();
  printk("  ram usage: %zu Kb (%zu b)\n", usage / KB, usage);
#endif

  process::ptr init = proc_table[1];
  assert(init);

  for (auto tid : p->threads) {
    auto *t = thread::lookup(tid);
    assert(t->state == PS_ZOMBIE);
#ifdef REAP_DEBUG
    printk(" [t:%d] sc:%d rc:%d\n", t->tid, t->stats.syscall_count,
           t->stats.run_count);
#endif

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

  pid_t res_pid = pid;

  while (1) {
    {
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
      if (options & WNOHANG) return -1;
    }

		// wait on the waiter's semaphore
    me->waiters.wait();
  }

  return res_pid;
}
