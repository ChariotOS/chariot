#include <cpu.h>
#include <fs/file.h>
#include <map.h>
#include <phys.h>
#include <printk.h>
#include <sched.h>
#include <task.h>
#include <template_lib.h>

constexpr const int fd_max = 255;

extern "C" void user_task_create_callback(void) {

  cpu::popcli();

  if (cpu::proc()->tasks.size() == 1) {
    // load the elf binary, since we're in the process now and have full access
    // to it's address space

    // setup argc, argv, etc...
    auto *t = cpu::task().get();

    unsigned long sp = 0;

#if defined(__ARCH_x86_64__) || defined(__ARCH_x86_64__)
    sp = t->tf->esp;
#endif

    // KERR("sp=%p\n", sp);

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define STACK_ALLOC(T, n)                \
  ({                                     \
    sp -= round_up(sizeof(T) * (n), 16); \
    (T *)(void *) sp;                    \
  })

    vec<char *> arg_storage;
    vec<char *> env_storage;

    auto argc = (u64)t->proc->args.size();

    // allocate space for each argument in the process,
    for (int i = 0; i < argc; i++) {
      string &arg = t->proc->args[i];
      auto p = STACK_ALLOC(char, arg.len() + 1);
      arg_storage.push(p);
      memcpy(p, arg.get(), arg.len() + 1);
    }

    // allocate space for argv
    auto argv = STACK_ALLOC(char *, argc + 1);

    for (int i = 0; i < arg_storage.size(); i++) {
      argv[i] = arg_storage[i];
    }
    argv[argc] = NULL;

    // KWARN("argc=%d, argv=%p\n", argc, argv);

    // argv goes into the second argument (RSI in x86_64)
    //
#if defined(__ARCH_x86_64__)
    t->tf->rdi = (unsigned long)argc;
    t->tf->rsi = (unsigned long)argv;
    t->tf->esp = sp;
#endif

#if defined(__ARCH_i386__)

    t->tf->edi = (unsigned long)argc;
    t->tf->esi = (unsigned long)argv;
    t->tf->esp = sp;
#endif
  }

  // tf->rdx = u_envp
}

extern "C" void trapret(void);
extern "C" void userret(void);
static void kernel_task_create_callback(void);

static spinlock proc_table_lock("proc table");
static u64 next_pid = 0;
static map<int, ref<struct task_process>> proc_table;

static spinlock task_table_lock("task table");
static u64 next_tid = 0;
static map<int, ref<struct task>> task_table;

task_process::task_process(void) {}

/**
 * task_process - create a task in a process
 *
 * takes in a function pointer, some flags, and an argument
 */
int task_process::create_task(int (*fn)(void *), int tflags, void *arg,
                              int state) {
  auto t = make_ref<task>(this);

  t->pid = pid;

  int ktask = (tflags & PF_KTHREAD) != 0;

  // 4 pages of kernel stack
  constexpr auto stksize = 4096 * 4;

  t->stack_size = stksize;
  t->stack = kmalloc(stksize);

  auto sp = (off_t)t->stack + stksize;

  sp -= sizeof(struct task_regs);
  t->tf = (struct task_regs *)sp;

  sp -= sizeof(u64);
  *(u64 *)sp = (u64)(ktask ? trapret : trapret);

  // initial context
  sp -= sizeof(*t->ctx);
  t->ctx = (struct task_context *)sp;
  memset(t->ctx, 0, sizeof(*t->ctx));

  t->flags = tflags;
  t->state = state;

  t->tf->esp = sp;
  t->tf->eip = (u64)fn;  // call the passed fn on ``iret''

  if (ktask) {
    // kernel ring
    t->tf->cs = (SEG_KCODE << 3) | DPL_KERN;
    t->tf->ds = (SEG_KDATA << 3) | DPL_KERN;
    t->tf->eflags = readeflags() | FL_IF;

    t->ctx->eip = (u64)kernel_task_create_callback;
  } else {
    // user ring
    t->tf->cs = (SEG_UCODE << 3) | DPL_USER;
    t->tf->ds = (SEG_UDATA << 3) | DPL_USER;
    t->tf->eflags = FL_IF;

    t->ctx->eip = (u64)user_task_create_callback;
  }

  task_table_lock.lock();
  t->tid = next_tid++;
  task_table[t->tid] = t;
  task_table_lock.unlock();

  tasks.push(t->tid);

  int add_res = sched::add_task(t);
  if (add_res != 0) {
    panic("failed to add task %d to the scheduler after creating\n", add_res);
  }

  return t->tid;
}

pid_t task_process::search_nursery(pid_t pid) {
  // TODO: lock nursery and make this optimized :)
  for (int i = 0; i < nursery.size(); i++) {
    if (pid == nursery[i]) {
      return pid;
    }
  }
  return -1;
}

void task_process::dump(void) {
  /* this whole function takes no locks, so take its output with a bit of salt */

  printk("process dump:\n");

  printk("   ticks: %zu\n", cpu::get_ticks());
  printk("   sched: rip=%p\n", cpu::current().sched_ctx->eip);
  for (auto &p : proc_table) {
    auto &proc = p.value;
    printk(" pid %d ", proc->pid);
    printk("ntasks=%-3d ", proc->tasks.size());

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
    printk("\n");

    for (auto tid : proc->tasks) {
      auto t = task::lookup(tid).get();

      printk("    ");
      printk("tid=%-3d ", tid, t->ticks);
      printk("ticks=%-4d ", t->ticks);
      const char *state = "UNKNOWN";
#define ST(name) \
  if (t->state == PS_##name) state = #name
      ST(UNRUNNABLE);
      ST(RUNNABLE);
      ST(ZOMBIE);
      ST(BLOCKED);
      ST(EMBRYO);
#undef ST

      printk("state=%-10s ", state);

      printk("pri=%-3d ", t->priority);
      printk("die=%-3d ", t->should_die);
      printk("SC=%-7d ", t->syscall_count);
      printk("RC=%-7d ", t->run_count);
      printk("timeslice=%-3d ", t->timeslice);
      printk("start=%-6d ", t->start_tick);
      printk("cpu=%-3d ", t->current_cpu);
      printk("last_cpu=%-3d ", t->last_cpu);

      printk("rip=%p ", t->tf->eip);
      printk("\n");
    }

    proc->mm.dump();

  }

  printk("\n");
}

ref<struct task_process> task_process::spawn(pid_t parent_pid, int &error) {
  proc_table_lock.lock();
  pid_t pid = next_pid++;

  assert(!proc_table.contains(pid));

  auto p = make_ref<task_process>();

  p->pid = pid;

  if (parent_pid != -1) {
    p->parent = proc_table.get(parent_pid);

    if (!p->parent) {
      error = -ENOENT;
      proc_table_lock.unlock();
      return nullptr;
    }

    p->uid = p->parent->uid;
    p->gid = p->parent->gid;
    p->ring = p->parent->ring;
  }
  // set the range of virtual memory that the mm can map
  // ... should be enough memory
  p->mm.set_range(0, 0x7ffffffff000);
  // p->mm.set_range(0, 0x10000000);

  p->create_tick = cpu::get_ticks();

  if (p->parent) {
    p->cwd = p->parent->cwd;
  }

  proc_table[pid] = p;
  proc_table_lock.unlock();

  error = 0;

  return p;
}

ref<task_process> task_process::kproc_init(void) {
  int err = 0;
  auto p = task_process::spawn(-1, err);

  p->flags = PF_KTHREAD;
  p->ring = 0;
  p->command_path = "kproc";

  // the kernel process is slightly wasteful, because processes allocate a page
  // table when they are created.
  // TODO: make this lazy
  p->mm.set_range(KERNEL_VIRTUAL_BASE, -1);

  phys::free(p->mm.cr3);

  p->mm.cr3 = v2p(get_kernel_page_table());
  p->mm.kernel_vm = true;

  return p;
}

ref<struct task_process> task_process::lookup(int pid) {
  proc_table_lock.lock();
  auto t = proc_table.get(pid);
  proc_table_lock.unlock();
  return t;
}

int task_process::open(const char *path, int flags, int mode) {
  int fd = -1;

  file_lock.lock();

  // search for a file descriptor!
  for (int i = 0; i < fd_max; i++) {
    if (!open_files.contains(i)) {
      fd = i;
      break;
    }

    if (!open_files[i]) {
      fd = i;
      break;
    }
  }

  if (fd == -1) {
    file_lock.unlock();
    return -1;
  }

  auto file = vfs::open(path, flags, mode);

  if (file == NULL) {
    file_lock.unlock();
    return -1;
  }

  open_files[fd].fd = fs::filedesc::create(file /* TODO: fd flags */, path);
  open_files[fd].flags = flags;

  file_lock.unlock();
  return fd;
}

int task_process::read(int fd, void *dst, size_t sz) {
  int n = -1;

  file_lock.lock();

  if (open_files.contains(fd)) {
    n = open_files[fd].fd->read(dst, sz);
  }

  file_lock.unlock();

  return n;
}

int task_process::write(int fd, void *data, size_t sz) {
  int n = -1;

  assert(readeflags() & FL_IF);
  file_lock.lock();

  if (open_files.contains(fd)) {
    n = open_files[fd].fd->write(data, sz);
  }
  file_lock.unlock();

  return n;
}

int task_process::close(int fd) {
  int n = -1;

  file_lock.lock();

  if (open_files.contains(fd)) {
    open_files[fd].clear();
  }

  file_lock.unlock();

  return n;
}

int task_process::do_dup(int oldfd, int newfd) {
  file_lock.lock();

  int fd = newfd;

  if (newfd == -1) {
    for (int i = 0; i < fd_max; i++) {
      if (!open_files.contains(i)) {
        fd = i;
        break;
      }

      if (!open_files[i]) {
        fd = i;
        break;
      }
    }
  }

  // do the dup
  // TODO: does this need more information?
  open_files[newfd] = open_files[oldfd];

  file_lock.unlock();
  return fd;
}

task::task(ref<struct task_process> proc) : proc(proc) {
  fpu_state = kmalloc(512);

  // default to the highest priority to maximize responsiveness of new tasks
  priority = PRIORITY_HIGH;
}

task::~task() {
  if (fpu_state != NULL) kfree(fpu_state);
}

void task::exit(int code) {
  // TODO: tell the process group that this task died :)
  should_die = true;
  exit_code = code;
  // rudely awaken the task if it needs to be.
  awaken(true);
}

ref<struct task> task::lookup(int tid) {
  task_table_lock.lock();
  auto t = task_table.get(tid);
  task_table_lock.unlock();
  return t;
}

static void kernel_task_create_callback(void) {


  auto task = cpu::task().get();

  using kfunc_t = int (*)(void *);
  kfunc_t kfn;
  kfn = (kfunc_t)task->tf->eip;

  cpu::popcli();

  kfn(nullptr);

  panic("unhandled: kthread finishes\n");
}

void fd_flags::clear() {
  fd = nullptr;
  flags = 0;
}

bool task::awaken(bool rudely) {
  if (!(wait_flags & WAIT_NOINT)) {
    if (rudely) {
      return false;
    }
  }

  // TODO: this should be more complex
  rudely_awoken = rudely;

  // fix up the wq double linked list
  current_wq = NULL;

  assert(state == PS_BLOCKED);
  state = PS_RUNNABLE;

  return true;
}
