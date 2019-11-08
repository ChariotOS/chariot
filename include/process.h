#pragma once

#include <fs/filedesc.h>
#include <func.h>
#include <lock.h>
#include <sched.h>
#include <string.h>
#include <syscalls.h>
#include <vm.h>

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

class thread;

struct fd_flags {
  inline operator bool() { return !!fd; }
  void clear();
  int flags;
  ref<fs::filedesc> fd;
};

class process final {
 public:
  process(string name, pid_t, gid_t, int ring = 3);
  ~process(void);

  inline pid_t pid(void) { return m_pid; }
  inline gid_t gid(void) { return m_gid; }
  inline int ring(void) { return m_ring; }

  const string &name(void);

  int m_ring;

  // for the intrusive linked list
  process *next;
  process *prev;

  void (*kernel_func)(void);

  /**
   * spawn a thread under this process, adding it to the global thread table
   */
  thread &create_thread(func<void(int tid)>);

  int do_close(int fd);
  int do_open(const char *, int flags, int mode = 0);
  ssize_t do_read(int fd, void *dst, size_t);
  ssize_t do_write(int fd, void *dst, size_t);
  off_t do_seek(int fd, off_t offset, int whence);

  int handle_pagefault(off_t faulting_addr, off_t *pte);

  // add a virtual memory region named `name` at a vpn, with len pages and prot
  // protection. It shall be backed by the memory mapping object
  int add_vm_region(string name, off_t vpn, size_t len, int prot,
                    unique_ptr<vm::memory_backing>);

 protected:
  vm::addr_space addr_space;
  vec<unique_ptr<thread>> threads;

  vec<fd_flags> files;

  string m_name;
  pid_t m_pid;
  gid_t m_gid;

  int next_tid = 0;
  mutex_lock big_lock;
};

class thread {
 public:
  ~thread();

  // for the scheduler's intrusive list
  thread *next;
  thread *prev;

  // TODO: move this to a thread context
  context_t *context;
  regs_t *tf;
  pstate state;

  u64 timeslice = 2;
  u64 start_tick = 0;

  int tid();

  inline process &proc(void) { return m_proc; }

  // called to start the thread from the scheduler
  void start(void);

  size_t nran = 0;

 protected:
  friend process;
  // only processes can craete threads
  thread(int tid, process &proc, func<void(int)> &);

  func<void(int tid)> kernel_func;
  void *kernel_stack;

  int m_tid;  // thread id
  process &m_proc;
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

void restart(void);
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

off_t lseek(int fd, off_t offset, int whence);

ssize_t read(int fd, void *, size_t);
ssize_t write(int fd, void *, size_t);

pid_t getpid(void);

pid_t gettid(void);

}  // namespace sys
