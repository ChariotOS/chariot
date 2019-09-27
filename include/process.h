#pragma once

#include <sched.h>
#include <string.h>
#include <syscalls.h>

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

enum pstate : u8 {
  UNUSED,
  EMBRYO,
  SLEEPING,
  BLOCKED,
  RUNNABLE,
  RUNNING,
  ZOMBIE
};

class process final {
 public:
  process(string name, pid_t, gid_t, int ring = 3);
  ~process(void);

  inline pid_t pid(void) { return m_pid; }
  inline gid_t gid(void) { return m_gid; }
  inline int ring(void) { return m_ring; }

  regs_t &regs(void);

  const string &name(void);

  // TODO: move this to a thread context
  context_t *context;
  regs_t *tf;
  pstate state;
  int m_ring;

  u64 timeslice = 2;

  u64 start_tick = 0;

  // for the intrusive linked list
  process *next;
  process *prev;

  void (*kernel_func)(void);

 protected:
  string m_name;
  pid_t m_pid;
  gid_t m_gid;

  void *kernel_stack;
};

void syscall_init(void);
long ksyscall(long n, ...);

#define SYSSYM(name) sys_##name

/**
 * the declaration of every syscall function. The kernel should go though this
 * interface to use them when running kernel processes/threads
 *
 * These functions are implemented in process.cpp
 */
namespace sys {

void exit(void);

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0100
#define O_EXCL 0200
#define O_NOCTTY 0400
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_DIRECTORY 00200000
#define O_NOFOLLOW 00400000
#define O_CLOEXEC 02000000
#define O_NOFOLLOW_NOERROR 0x4000000
int open(const char *path, int flags, int mode = 0);


int close(int fd);


}  // namespace sys
