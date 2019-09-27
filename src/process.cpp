#include <cpu.h>
#include <errno.h>
#include <mem.h>
#include <phys.h>
#include <process.h>
#include <syscalls.h>

extern "C" void trapret(void);

process::process(string name, pid_t pid, gid_t gid, int ring)
    : m_ring(ring), m_name(name), m_pid(pid), m_gid(gid) {
  int stksize = 4096 * 6;
  kernel_stack = kmalloc(stksize);

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

process::~process(void) { kfree(kernel_stack); }

regs_t &process::regs(void) { return *tf; }

const string &process::name(void) { return m_name; }

long sys_invalid(u64, u64, u64, u64, u64, u64) { return 0; }

#define SYSSYM(name) sys_##name

void sys::exit() {
  // TODO: NEED TO EXIT
}

int sys::open(const char *path, int flags, int mode) {
  KINFO("path: '%s', mode=%d, flags=%d\n", path, mode, flags);
  return -ENOTIMPL;
}

int sys::close(int fd) { return -ENOTIMPL; }

static const char *syscall_names[] = {
#undef __SYSCALL
#define __SYSCALL(num, name) [num] = #name,
#include <syscalls.inc>
};

static void *syscall_table[] = {
// [0 ... SYS_COUNT] = (void *)sys_invalid,

#undef __SYSCALL
#define __SYSCALL(num, name) [num] = (void *)sys::name,
#include <syscalls.inc>
};

void syscall_init(void) {}

static long do_syscall(long num, u64 a, u64 b, u64 c, u64 d, u64 e) {
  if (num >= SYS_COUNT) {
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
