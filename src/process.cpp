#include <cpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <mem.h>
#include <phys.h>
#include <process.h>
#include <syscalls.h>
#include <asm.h>
#include <vga.h>
#include <util.h>

extern "C" void trapret(void);

void fd_flags::clear() {
  fd = nullptr;
  flags = 0;
}

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
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}

int sys::close(int fd) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}


off_t sys::lseek(int fd, off_t offset, int whence) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}

ssize_t sys::read(int fd, void *dst, size_t len) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}

ssize_t sys::write(int fd, void *dst, size_t len) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}


int sys::yield(void) {
  sched::yield();
  return 0;
}

// XXX: REMOVE
int sys::set_pixel(int x, int y, int color) {
  vga::set_pixel(x, y, color);
  return 0;
}

pid_t sys::getpid(void) {
  return cpu::task()->pid;
}

pid_t sys::gettid(void) {
  return cpu::task()->tid;
}



pid_t sys::spawn(void) {

  assert(cpu::in_thread());
  auto proc = cpu::proc();



  return -1;
}


int sys::impersonate(pid_t) {
  return -ENOTIMPL;
}


int sys::cmdpidve(pid_t pid, const char *abs_path, const char *argv[], const char *envp[]) {
  return -1;
}






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


  /*
  KINFO("syscall(%s, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx)\n",
        syscall_names[num], a, b, c, d, e);
        */

  auto *func = (long (*)(u64, u64, u64, u64, u64))syscall_table[num];

  return func(a, b, c, d, e);
}



void syscall_handle(int i, struct task_regs *tf) {


  // int x = 0;
  // printk("rax=%p krsp~%p\n", tf->rax, &x);

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
