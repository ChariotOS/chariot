#include <cpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <mem.h>
#include <phys.h>
#include <process.h>
#include <syscalls.h>
#include <asm.h>

extern "C" void trapret(void);

void fd_flags::clear() {
  fd = nullptr;
  flags = 0;
}




process::process(string name, pid_t pid, gid_t gid, int ring)
    : m_ring(ring),
      m_name(name),
      m_pid(pid),
      m_gid(gid),
      big_lock("processlock") {
  if (m_ring == RING_KERNEL) {
    addr_space.page_table = get_kernel_page_table();
  } else {
    addr_space.page_table = p2v(phys::alloc(1));
  }
}

process::~process(void) {
  // TODO: deallocate the address space
}

const string &process::name(void) { return m_name; }

thread &process::create_thread(func<void(int tid)> f) {
  big_lock.lock();

  // construct the new thread
  auto thd = unique_ptr(new thread(sched::next_pid(), *this, f));
  thread &t = *thd;

  // first, we simply walk over the list of threads, looking for a spot that a
  // thread got removed from.
  for (int i = 0; i < threads.size(); i++) {
    if (!threads[i]) {
      threads[i] = move(thd);
      big_lock.unlock();
      return t;
    }
  }

  threads.push(move(thd));

  big_lock.unlock();
  return t;
}

int process::do_close(int fd) {
  // handle invalid file desc indexes
  if (fd > files.size() || fd < 0) return -1;

  // not a valid file
  if (!files[fd]) return -1;

  // clear out the fd
  big_lock.lock();
  files[fd].clear();
  big_lock.unlock();
  return 0;  // success
}

int process::do_open(const char *path, int flags, int mode) {
  // TODO: handle flags and mode!

  // printk("[OPEN] looking for %s\n", path);
  auto file = vfs::open(path);

  if (file.get() == nullptr) return -1;

  int fdflags = 0;
  if (flags & O_RDONLY) fdflags |= FDIR_READ;
  if (flags & O_WRONLY) fdflags |= FDIR_WRITE;
  if (flags & O_RDWR) fdflags |= FDIR_READ | FDIR_WRITE;

  auto fd = make_ref<fs::filedesc>(file, fdflags);

  // lock after the file is opened and the file was found
  big_lock.lock();
  // look through the file vector for a free space (reuse fds)
  for (int i = 0; i < files.size(); i++) {
    if (!files[i]) {
      files[i].fd = fd;
      files[i].flags = flags;
      big_lock.unlock();
      return i;
    }
  }

  // say the proc took longer than it did
  cpu::thd().start_tick--;
  // there was no unused spaces
  files.push({.flags = flags, .fd = fd});
  big_lock.unlock();

  return files.size() - 1;
}

/**
 * whenever a pagefault happens, the PGFLT handler forwards here, into the
 * faulting process
 */
int process::handle_pagefault(off_t faulting_addr, off_t *pte) {
  int code = -1;
  big_lock.lock();

  big_lock.unlock();
  return code;
}

int process::add_vm_region(string name, off_t vpn, size_t len, int prot,
                           unique_ptr<vm::memory_backing>) {
  printk("adding region '%s' to %p\n", name.get(), (void *)(vpn << 12));
  return 0;
}

ssize_t process::do_read(int fd, void *dst, size_t len) {
  // TODO: handle permissions, EOF, modes, etc..

  // handle invalid file desc indexes
  if (fd > files.size() || fd < 0) return -1;

  // not a valid file
  if (!files[fd]) return -1;

  // clear out the fd
  big_lock.lock();

  ssize_t ret = files[fd].fd->read(dst, len);
  big_lock.unlock();
  return ret;
}

ssize_t process::do_write(int fd, void *dst, size_t len) {
  // TODO: handle permissions, EOF, modes, etc..

  // handle invalid file desc indexes
  if (fd > files.size() || fd < 0) return -1;

  // not a valid file
  if (!files[fd]) return -1;

  // clear out the fd
  big_lock.lock();

  ssize_t ret = files[fd].fd->write(dst, len);
  big_lock.unlock();
  return ret;
}

off_t process::do_seek(int fd, off_t offset, int whence) {
  // TODO: handle permissions, EOF, modes, etc..

  // handle invalid file desc indexes
  if (fd > files.size() || fd < 0) return -1;

  // not a valid file
  if (!files[fd]) return -1;

  // clear out the fd
  big_lock.lock();

  ssize_t ret = files[fd].fd->seek(offset, whence);
  big_lock.unlock();
  return ret;
}

void process::switch_vm(void) {
  // printk("switch into %p\n", addr_space.page_table);

  if (addr_space.page_table == nullptr) {
    panic("null page table!\n");
  }
  write_cr3((u64)v2p(addr_space.page_table));
}

/**
 * ==============================================
 * begin thread functions
 */
thread::thread(int tid, process &proc, func<void(int)> &kfunc)
    : kernel_func(kfunc), m_tid(tid), m_proc(proc) {
  int stksize = 4096 * 6;
  kernel_stack = kmalloc(stksize);

  // the initial stack pointer is at the end of the stack memory
  auto sp = (u64)kernel_stack + stksize;

  // leave room for a trap frame
  sp -= sizeof(regs_t);
  tf = (regs_t *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= sizeof(u64);
  *(u64 *)sp = (u64)trapret;

  // setup initial context
  sp -= sizeof(regs_t);
  context = (context_t *)sp;

  memset(context, 0, sizeof(regs_t));

  state = pstate::EMBRYO;
}

thread::~thread(void) { kfree(kernel_stack); }

void thread::start(void) {
  // just forward to the kernel func
  kernel_func(tid());
}

int thread::tid(void) { return m_tid; }

/**
 * ===============================================
 * end thread functions
 * begin system calls
 */

// static long sys_invalid(u64, u64, u64, u64, u64, u64) { return 0; }

#define SYSSYM(name) sys_##name

void sys::restart() {
  // TODO: NEED TO RESTART
}

void sys::exit() {
  // TODO: NEED TO EXIT
}

int sys::open(const char *path, int flags, int mode) {
  // TODO: verify the string passed, as to not read invalid memory
  return cpu::proc().do_open(path, flags, mode);
}

int sys::close(int fd) { return cpu::proc().do_close(fd); }

off_t sys::lseek(int fd, off_t offset, int whence) {
  return cpu::proc().do_seek(fd, offset, whence);
}

ssize_t sys::read(int fd, void *dst, size_t len) {
  // TODO: CHECK FOR BUFFER/ADDR SPACE VALIDITY
  return cpu::proc().do_read(fd, dst, len);
}

ssize_t sys::write(int fd, void *dst, size_t len) {
  // TODO: CHECK FOR BUFFER/ADDR SPACE VALIDITY
  return cpu::proc().do_write(fd, dst, len);
}

pid_t sys::getpid(void) { return cpu::proc().pid(); }

pid_t sys::gettid(void) { return cpu::thd().tid(); }

static const char *syscall_names[] = {
#undef __SYSCALL
#define __SYSCALL(num, name) [num] = #name,
#include <syscalls.inc>
};

// WARNING: HACK

static void *syscall_table[] = {
// [0 ... SYS_COUNT] = (void *)sys_invalid,

#undef __SYSCALL
#define __SYSCALL(num, name) [num] = (void *)sys::name,
#include <syscalls.inc>
};

void syscall_init(void) {
  for (int i = 0; i < SYS_COUNT; i++) {
    // printk("%d: %p\n", i, syscall_table);
  }
}

static long do_syscall(long num, u64 a, u64 b, u64 c, u64 d, u64 e) {
  if (num >= SYS_COUNT) {
    return -1;
  }

  if (syscall_table[num] == 0) {
    return -1;
  }

  KINFO("syscall(%s, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx)\n",
        syscall_names[num], a, b, c, d, e);

  auto *func = (long (*)(u64, u64, u64, u64, u64))syscall_table[num];

  return func(a, b, c, d, e);
}

void syscall_handle(int i, regs_t *tf) {
  // grab the number out of rax
  tf->rax = do_syscall(tf->rax, tf->rdi, tf->rsi, tf->rdx, tf->r10, tf->r8);
  return;
}

extern "C" long __syscall(size_t, size_t, size_t, size_t, size_t, size_t,
                          size_t);

long ksyscall(long n, ...) {
  va_list ap;
  size_t a, b, c, d, e;
  va_start(ap, n);
  a = va_arg(ap, size_t);
  b = va_arg(ap, size_t);
  c = va_arg(ap, size_t);
  d = va_arg(ap, size_t);
  e = va_arg(ap, size_t);
  va_end(ap);

  return do_syscall(n, a, b, c, d, e);
}
