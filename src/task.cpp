#include <cpu.h>
#include <fs/file.h>
#include <map.h>
#include <printk.h>
#include <sched.h>
#include <task.h>

extern "C" void user_task_create_callback(void) {

  printk("starting user task\n");
  cpu::popcli();
}

extern "C" void trapret(void);
extern "C" void userret(void);
static void kernel_task_create_callback(void);

static mutex_lock proc_table_lock("process_table");
static u64 next_pid = 0;
static map<int, ref<struct task_process>> proc_table;

static mutex_lock task_table_lock("task_table");
static u64 next_tid = 0;
static map<int, ref<struct task>> task_table;

task_process::task_process(void) : proc_lock("proc_lock") {}

/**
 * task_process - create a task in a process
 *
 * takes in a function pointer, some flags, and an argument
 */
int task_process::create_task(int (*fn)(void *), int tflags, void *arg) {
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
  t->state = PS_RUNNABLE;


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
    t->tf->esp =  0x1000 + PGSIZE - 1;

  }
  printk("%p %p %p\n", t->tf->cs, t->tf->ds, t->tf->eflags);


  task_table_lock.lock();
  t->tid = next_tid++;
  task_table[t->tid] = t;
  task_table_lock.unlock();

  int add_res = sched::add_task(t);
  if (add_res != 0) {
    panic("failed to add task %d to the scheduler after creating\n", add_res);
  }

  return t->tid;
}

ref<struct task_process> task_process::spawn(string path, int uid, int gid,
                                             pid_t parent_pid, int &error,
                                             vec<string> &&args, int spwn_flags,
                                             int ring) {
  proc_table_lock.lock();
  pid_t pid = next_pid++;

  KINFO("spawned process '%s' at pid %d user=(%d,%d)\n", path.get(), pid, uid,
        gid);

  assert(!proc_table.contains(pid));

  auto p = make_ref<task_process>();

  p->uid = uid;
  p->gid = gid;
  p->ring = ring;

  auto cmd_path = fs::path(path);

  p->command_path = move(path);
  p->args = args;

  p->parent = proc_table.get(parent_pid);

  p->mm = make_ref<vm::addr_space>();

  if (p->parent) {
    p->cwd = p->parent->cwd;
  }

  proc_table[pid] = p;
  proc_table_lock.unlock();

  return p;
}

ref<struct task_process> task_process::lookup(int pid) {
  proc_table_lock.lock();
  auto t = proc_table.get(pid);
  proc_table_lock.unlock();
  return t;
}

task::task(ref<struct task_process> proc) : proc(proc), task_lock("task lock") {}




ref<struct task> task::lookup(int tid) {
  task_table_lock.lock();
  auto t = task_table.get(tid);
  task_table_lock.unlock();
  return t;
}

static void kernel_task_create_callback(void) {
  auto task = cpu::task();

  cpu::popcli();


  printk("task %d is kthread\n", task->tid);

  using kfunc_t = int (*)(void *);
  kfunc_t kfn;
  kfn = (kfunc_t)task->tf->eip;

  kfn(nullptr);

  panic("unhandled: kthread finishes\n");
}
