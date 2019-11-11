#pragma once

#include <atom.h>
#include <fs/vfs.h>
#include <lock.h>
#include <types.h>
#include <vm.h>

#define BIT(n) (1 << (n))

struct task_regs {
  u64 rax;
  u64 rbx;
  u64 rcx;
  u64 rdx;
  u64 rbp;
  u64 rsi;
  u64 rdi;
  u64 r8;
  u64 r9;
  u64 r10;
  u64 r11;
  u64 r12;
  u64 r13;
  u64 r14;
  u64 r15;

  u64 trapno;
  u64 err;

  u64 eip;  // rip
  u64 cs;
  u64 eflags;  // rflags
  u64 esp;     // rsp
  u64 ds;      // ss
};

struct task_context {
  u64 r15;
  u64 r14;
  u64 r13;
  u64 r12;
  u64 r11;
  u64 rbx;
  u64 ebp;  // rbp
  u64 eip;  // rip;
};

struct task_process final : public refcounted<task_process> {
  int pid;

  /* address space information */
  vm::addr_space *mm = nullptr;

  /* cwd - current working directory
   */
  ref<fs::vnode> cwd;

  // create a thread in the task_process
  ref<struct task> clone(int (*fn)(void *), void *stack, int falgs, void *arg);

  static ref<struct task_process> create(void);
  static ref<struct task_process> lookup(int pid);
};

/*
 * Per process flags
 *
 * These are mostly stollen from linux in include/linux/sched.h
 */
#define PF_IDLE BIT(0)     // is an idle thread
#define PF_EXITING BIT(1)  // in the process of exiting
#define PF_KTHREAD BIT(2)  // is a kernel thread
#define PF_MAIN BIT(3)     // this task is the main task for a process

/**
 * task - a schedulable entity in the kernel
 */
struct task final : public refcounted<task> {
  /* task id */
  int tid;
  /* process id */
  int pid;

  /* -1 unrunnable, 0 runnable, >0 stopped */
  volatile long state;

  /* kenrel stack */
  void *stack;

  /* per-task flasg (uses PF_* macros)*/
  unsigned int flags;

  struct task_process &proc;

  /* the current cpu this task is running on */
  atom<int> current_cpu;
  /* the previous cpu */
  atom<int> last_cpu;

  // used when an action requires ownership of this task
  mutex_lock task_lock;

  struct task_context *ctx;
  struct task_regs *tf;

  // how many times the task has been ran
  unsigned long ticks = 0;

  // for the scheduler's intrusive runqueue
  struct task *next = nullptr;
  struct task *prev = nullptr;

  static ref<struct task> create(void);
  static ref<struct task> lookup(int tid);

 protected:
  // protected constructor - must use ::create
  task(struct task_process &);
};
