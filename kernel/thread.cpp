/**
 * This file implements all the thread:: functions and methods
 */
#include <cpu.h>
#include <mmap_flags.h>
#include <process.h>
#include <sched.h>
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
extern "C" void user_initial_trapret(int argc, char **argv, char **envp);

static spinlock thread_table_lock;
static map<pid_t, struct thread *> thread_table;

thread::thread(pid_t tid, struct process &proc) : proc(proc) {
  this->tid = tid;

  this->pid = proc.pid;

  fpu.initialized = false;
  fpu.state = kmalloc(512);

  sched.priority = PRIORITY_HIGH;

  stack_size = PGSIZE * 16;
  stack = kmalloc(stack_size);

  auto sp = (off_t)stack + stack_size;

  // get a pointer to the trapframe on the stack
  sp -= sizeof(struct regs);
  trap_frame = (struct regs *)sp;

  // 'return' to trapret()
  sp -= sizeof(void *);
  *(void **)sp = (void *)trapret;

  // initial kernel context
  sp -= sizeof(struct thread_context);
  kern_context = (struct thread_context *)sp;
  memset(kern_context, 0, sizeof(struct thread_context));

  state = PS_EMBRYO;

  trap_frame->esp = sp;
  trap_frame->eip = -1;  // to be set in ::kickoff()

  // set the initial context to the creation boostrap function
  kern_context->eip = (u64)thread_create_callback;

  // setup segments (ring permissions)
  if (proc.ring == RING_KERN) {
    trap_frame->cs = (SEG_KCODE << 3) | DPL_KERN;
    trap_frame->ds = (SEG_KCODE << 3) | DPL_KERN;
    trap_frame->eflags = readeflags() | FL_IF;
  } else if (proc.ring == RING_USER) {
    trap_frame->cs = (SEG_UCODE << 3) | DPL_USER;
    trap_frame->ds = (SEG_UDATA << 3) | DPL_USER;
    trap_frame->eflags = FL_IF;
  } else {
    panic("Unknown ring %d\n", proc.ring);
  }

  thread_table_lock.lock();
  assert(!thread_table.contains(tid));
  // printk("inserting %d\n", tid);
  thread_table.set(tid, this);
  thread_table_lock.unlock();

  // push the tid into the proc's tid list
  proc.threads.push(tid);
}

bool thread::kickoff(void *rip, int initial_state) {
  trap_frame->eip = (unsigned long)rip;

  this->state = initial_state;

  sched::add_task(this);
  return true;
}

thread::~thread(void) {
  panic("Thread [t:%d,p:%d] Deleted in %p!\n", tid, pid,
        __builtin_return_address(0));
  kfree(stack);
  kfree(fpu.state);

  // TODO: remove this thread from the process
}

bool thread::awaken(bool rudely) {
  if (!(wq.flags & WAIT_NOINT)) {
    if (rudely) {
      return false;
    }
  }

  // TODO: this should be more complex
  wq.rudely_awoken = rudely;

  // fix up the wq double linked list
  wq.current_wq = NULL;

  assert(state == PS_BLOCKED);
  state = PS_RUNNABLE;

  return true;
}

static void thread_create_callback(void *) {
  auto thd = curthd;

  auto tf = thd->trap_frame;

  if (thd->proc.ring == RING_KERN) {
    using fn_t = int (*)(void *);
    auto fn = (fn_t)thd->trap_frame->eip;
    cpu::popcli();
    // run the kernel thread
    int res = fn(NULL);
    // exit the thread with the return code of the func
    sys::exit_thread(res);
  } else {
    if (thd->pid == thd->tid) {
      auto sp = tf->esp;
#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define STACK_ALLOC(T, n)               \
  ({                                    \
    sp -= round_up(sizeof(T) * (n), 8); \
    (T *)(void *) sp;                   \
  })
      auto argc = (u64)thd->proc.args.size();

      size_t sz = 0;
      sz += thd->proc.args.size() * sizeof(char *);
      for (auto &a : thd->proc.args) sz += a.size() + 1;

      auto region = (void *)STACK_ALLOC(char, sz);
      // auto region = thd->proc.addr_space->add_mapping("[argv]", round_up(sz,
      // 4096), VPROT_READ | VPROT_WRITE);

      auto argv = (char **)region;

      auto *arg = (char *)((char **)argv + argc);

      int i = 0;
      for (auto &a : thd->proc.args) {
        int len = a.size() + 1;
        memcpy(arg, a.get(), len);
        argv[i++] = arg;
        arg += len;
      }


      auto envc = thd->proc.env.size();
      auto envp = STACK_ALLOC(char *, envc + 1);

      for (int i = 0; i < envc; i++) {
        auto &e = thd->proc.env[i];
        envp[i] = STACK_ALLOC(char, e.len() + 1);
        memcpy(envp[i], e.get(), e.len() + 1);
      }

      envp[envc + 1] = NULL;


      tf->esp = sp;
      tf->rdi = argc;
      tf->rsi = (unsigned long)argv;
      tf->rdx = (unsigned long)envp;
    }
    cpu::popcli();

    return;
  }

  sys::exit_proc(-1);
  while (1) {
  }
}

struct thread *thread::lookup(pid_t tid) {
  thread_table_lock.lock();
  assert(thread_table.contains(tid));
  auto t = thread_table.get(tid);
  thread_table_lock.unlock();
  return t;
}
