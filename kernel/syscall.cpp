#include <asm.h>
#include <cpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <map.h>
#include <mem.h>
#include <mmap_flags.h>
#include <paging.h>
#include <pctl.h>
#include <phys.h>
#include <syscall.h>
#include <syscalls.h>
#include <util.h>
#include <vga.h>

extern "C" void wrmsr(u32 msr, u64 val);

extern "C" void trapret(void);

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

pid_t sys::getpid(void) { return curthd->pid; }

pid_t sys::gettid(void) { return curthd->tid; }

// WARNING: HACK
struct syscall {
  const char *name;
  int num;
  void *handler;
};

struct syscall syscall_table[255];

void set_syscall(const char *name, int num, void *handler) {
  // KINFO("%s -> %d (0x%02x)\n", name, num, num);
  syscall_table[num] = {.name = name, .num = num, .handler = handler};
}

void syscall_init(void) {
#undef __SYSCALL
#define __SYSCALL(num, name) set_syscall(#name, num, (void *)sys::name);
#include <syscalls.inc>
}

static u64 do_syscall(long num, u64 a, u64 b, u64 c, u64 d, u64 e, u64 f) {
  if (num & !0xFF) return -1;

  if (syscall_table[num].handler == NULL) {
    KWARN("unknown syscall in pid %d. syscall(%d) @ rip=%p\n", curthd->pid, num,
          curthd->trap_frame->eip);
    return -1;
  }

  /*
  printk("%d '%s' 0x%02x\n", curproc->pid, syscall_table[num].name, num);
  */

  curthd->stats.syscall_count++;

  auto *func = (u64(*)(u64, u64, u64, u64, u64, u64))syscall_table[num].handler;

  return func(a, b, c, d, e, f);
}

extern "C" void syscall_handle(int i, struct regs *tf) {
  // int x = 0;
  //
#ifdef __ARCH_x86_64__
  tf->rax =
      do_syscall(tf->rax, tf->rdi, tf->rsi, tf->rdx, tf->r10, tf->r8, tf->r9);
#endif

#ifdef __ARCH_i386__
  tf->eax = do_syscall(tf->eax, tf->ebx, tf->ecx, tf->edx, tf->esi, tf->edi,
                       0 /* TODO */);
#endif
  return;
}

extern "C" long __syscall(size_t, size_t, size_t, size_t, size_t, size_t,
                          size_t);

long ksyscall(long n, ...) {
  va_list ap;
  size_t a, b, c, d, e, f;
  va_start(ap, n);
  a = va_arg(ap, size_t);
  b = va_arg(ap, size_t);
  c = va_arg(ap, size_t);
  d = va_arg(ap, size_t);
  e = va_arg(ap, size_t);
  f = va_arg(ap, size_t);
  va_end(ap);

  return do_syscall(n, a, b, c, d, e, f);
}
