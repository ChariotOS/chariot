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

  void switch_vm(void);

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

int yield(void);

pid_t getpid(void);

pid_t gettid(void);

// spawn a new USER process, inheriting the stdin, stdout, stderr from the
// calling process.
pid_t spawn();

// TODO:
int impersonate(pid_t);

// command an embryo process to execute a command and promote it from embryo to
// runnable. It will have a thread created for it and will be a user process.
int cmdpidve(pid_t, const char *abs_path, const char *argv[],
             const char *envp[]);

// XXX: REMOVE
int set_pixel(int x, int y, int color);



void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset);
int munmap(void *addr, size_t length);

}  // namespace sys
