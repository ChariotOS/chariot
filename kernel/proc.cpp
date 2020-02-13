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

// start out at pid 2, so init is pid 1 regardless of if kernel threads are
// created before init is spawned
pid_t next_pid = 2;
static spinlock pid_spinlock;
static spinlock proc_table_lock;
static map<pid_t, process::ptr> proc_table;

static pid_t get_next_pid(void) {
  pid_spinlock.lock();
  auto p = next_pid++;
  pid_spinlock.unlock();
  return p;
}

static vm::addr_space *alloc_user_vm(void) {
  auto a = new vm::addr_space;

  a->set_range(0x1000, 0x7ffffffff000);
  return a;
}

static process::ptr do_spawn_proc(process::ptr proc_ptr,
                                  struct sched::proc::spawn_options &opts) {
  // get a reference, they are nicer to look at.
  // (and we spend less time in ref<process>::{ref,deref}())
  auto &proc = *proc_ptr;

  scoped_lock lck(proc.datalock);

  proc.create_tick = cpu::get_ticks();
  proc.embryonic = true;

  proc.ring = opts.ring;

  // This check is needed because the kernel process has no parent.
  if (proc.parent) {
    proc.user = proc_ptr->parent->user;  // inherit the user information
    proc.cwd = proc_ptr->parent->cwd;    // inherit cwd (makes sense)
    proc.ppid = proc_ptr->parent->pid;
    proc.pgid = proc_ptr->parent->pgid;  // inherit the group id of the parent

    // inherit stdin(0) stdout(1) and stderr(2)
    for (int i = 0; i < 3; i++) {
      proc.open_files[i] = proc_ptr->parent->get_fd(i);
    }
  }

  if (proc.cwd == NULL) {
    proc.cwd = vfs::get_root();
  }
  // special case for kernel's init
  if (proc.cwd != NULL) fs::inode::acquire(proc.cwd);

  proc.addr_space = alloc_user_vm();
  return proc_ptr;
}

/**
 * Internally, there is no "process table" that keeps track of processes. They
 * are only accessed (and owned) by the tasks that are a member of them. Very
 * democratic. This assures that the lifetime of a process is no longer than
 * that of the constituent tasks.
 */
process::ptr sched::proc::spawn_process(struct process *parent,
                                        struct spawn_options &opts) {
  // grab the next pid (will be the tid of the main thread when it is executed)
  pid_t pid = get_next_pid();

  // allocate the process
  proc_table_lock.lock();
  auto proc_ptr = make_ref<process>();
  proc_table.set(pid, proc_ptr);
  proc_table_lock.unlock();
  if (proc_ptr.get() == NULL) return nullptr;

  proc_ptr->pid = pid;
  proc_ptr->pgid = pid;  // pgid == pid (if no parent)

  // Set up initial data for the process.
  proc_ptr->parent = parent;
  return do_spawn_proc(proc_ptr, opts);
};

bool sched::proc::ptable_remove(pid_t p) {
  bool succ = false;

  proc_table_lock.lock();
  if (proc_table.contains(p)) {
    proc_table.remove(p);
    succ = true;
  }
  proc_table_lock.unlock();

  return succ;
}

pid_t sched::proc::spawn_init(vec<string> &paths) {
  pid_t pid = 1;

  process::ptr proc_ptr;

  {
    scoped_lock lck(proc_table_lock);
    proc_ptr = make_ref<process>();
    if (proc_ptr.get() == NULL) return -1;

    proc_ptr->pid = pid;
    proc_ptr->pgid = pid;  // pgid == pid (if no parent)

    assert(!proc_table.contains(pid));
    proc_table.set(pid, proc_ptr);
  }

  proc_ptr->parent = sched::proc::kproc();

  struct spawn_options opts;
  opts.ring = RING_USER;

  // initialize normally
  proc_ptr = do_spawn_proc(proc_ptr, opts);

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
    struct sched::proc::spawn_options opts;
    opts.ring = RING_KERN;

    kernel_process = sched::proc::spawn_process(nullptr, opts);
    kernel_process->pid = 0;  // just lower than init (pid 1)
    kernel_process->embryonic = false;
    auto &vm = kernel_process->addr_space;

    vm->set_range(KERNEL_VIRTUAL_BASE, -1);
    phys::free(vm->cr3);
    vm->cr3 = v2p(get_kernel_page_table());
    vm->kernel_vm = true;
  }

  return kernel_process.get();
}

pid_t sched::proc::create_kthread(int (*func)(void *), void *arg) {
  auto proc = kproc();

  auto tid = get_next_pid();

  // construct the thread
  auto thd = new thread(tid, *proc);
  thd->trap_frame->rdi = (unsigned long)arg;

  thd->kickoff((void *)func, PS_RUNNABLE);

  return tid;
}

ref<fs::filedesc> process::get_fd(int fd) {
  ref<fs::filedesc> file;

  scoped_lock l(file_lock);
  auto it = open_files.find(fd);
  if (it != open_files.end()) {
    file = it->value;
  }

  return file;
}

int process::add_fd(ref<fs::filedesc> file) {
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
  delete addr_space;
}

void sched::proc::dump_table(void) {
  printk("process dump:\n");

  printk("   ticks: %zu\n", cpu::get_ticks());
  printk("   sched: rip=%p\n", cpu::current().sched_ctx->eip);
  for (auto &p : proc_table) {
    auto &proc = p.value;
    printk("pid %d ", proc->pid);
    printk("threadc=%-3d ", proc->threads.size());

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

      printk("rip=%p ", t->trap_frame->eip);
      printk("\n");
    }

    proc->addr_space->dump();
    printk("\n");
  }
}

int process::exec(string &path, vec<string> &argv, vec<string> &envp) {
  scoped_lock lck(this->datalock);

  /*
  printk("Attempting exec()\n");
  printk("   path='%s'\n", path.get());

  printk("   argv=[");
  for (int i = 0; i < argv.size(); i++) {
    printk("'%s'", argv[i].get());
    if (i < argv.size() - 1) printk(", ");
  }
  printk("]\n");

  printk("   envp=[");
  for (int i = 0; i < envp.size(); i++) {
    printk("'%s'", envp[i].get());
    if (i < envp.size() - 1) printk(", ");
  }
  printk("]\n");
  */

  if (!this->embryonic) return -1;

  if (threads.size() != 0) return -1;

  // try to load the binary
  fs::inode *exe = NULL;

  // TODO: open permissions on the binary
  if (vfs::namei(path.get(), 0, 0, cwd, exe) != 0) return -ENOENT;
  // TODO check execution permissions

  /*
  printk("found the binary!\n");
  // wipe out threads and address space
  for (auto t : threads) {
    printk("t=%d\n", t);
    auto *thd = thread::lookup(t);
    assert(thd != NULL);

    printk("must teardown thread %p [tid=%d]\n", thd, t);
  }
  */

  off_t entry = 0;
  auto fd = fs::filedesc(exe, FDIR_READ);

  // allocate a new address space
  auto *new_addr_space = alloc_user_vm();

  int loaded = elf::load(path.get(), *new_addr_space, fd, entry);

  if (loaded != 0) {
    delete new_addr_space;
    return -ENOEXEC;
  }

  off_t stack = 0;
  // TODO: this size is arbitrary.
  auto stack_size = 8 * PGSIZE;

  stack = new_addr_space->add_mapping("[stack]", stack_size,
                                      VPROT_READ | VPROT_WRITE);

  // TODO: push argv and arguments onto the stack
  this->args = argv;
  this->env = envp;

  // delete new_addr_space;
  //
  //
  delete this->addr_space;
  this->addr_space = new_addr_space;

  // printk("entry=%p\n", entry);
  // printk("stack=%p\n", stack);
  // printk("new addr space = %p\n", new_addr_space);
  //
  this->embryonic = false;

  // construct the thread
  auto thd = new thread(pid, *this);
  thd->trap_frame->esp = stack + stack_size - 32;
  thd->kickoff((void *)entry, PS_RUNNABLE);

  return 0;
}

