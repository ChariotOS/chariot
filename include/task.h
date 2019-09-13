#pragma once

#include <sched.h>
#include <string.h>

#define RING_KERNEL 0
#define RING_USER 3

struct regs_t {
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

struct context_t {
  u64 r15;
  u64 r14;
  u64 r13;
  u64 r12;
  u64 r11;
  u64 rbx;
  u64 ebp;  // rbp
  u64 eip;  // rip;
};

// a task is an abstract version of a process in other systems. Kernel threads,
// user processes, etc. are all tasks
class task final {
 public:
  task(string name, pid_t, gid_t, int ring = 3);
  ~task(void);

  enum state : u8 { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

  inline pid_t pid(void) { return m_pid; }
  inline gid_t gid(void) { return m_gid; }
  inline int ring(void) { return m_ring; }

  regs_t &regs(void);

  const string &name(void);

  // TODO: move this to a thread context
  context_t *context;
  regs_t *tf;
  task::state state;
  int m_ring;

  u64 timeslice = 2;

  u64 start_tick = 0;


  
  void (*kernel_func)(void);

 protected:
  string m_name;
  pid_t m_pid;
  gid_t m_gid;

  void *kernel_stack;
};
