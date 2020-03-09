#include <asm.h>
#include <cpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <map.h>
#include <mem.h>
#include <mmap_flags.h>
#include <pctl.h>
#include <phys.h>
#include <syscall.h>
#include <syscalls.h>
#include <util.h>
#include <vga.h>

extern "C" void wrmsr(u32 msr, u64 val);

extern "C" void trapret(void);

#define SYSSYM(name) sys_##name

void sys::restart() {}

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
  assert(num >= 0 && num < 255);
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
    return -1;
  }

  curthd->stats.syscall_count++;

  auto *func = (u64(*)(u64, u64, u64, u64, u64, u64))syscall_table[num].handler;

  auto res = func(a, b, c, d, e, f);

#if 0
  printk("pid: %d, syscall(SYS_%s, %lx, %lx, %lx, %lx, %lx, %lx) = %lx\n",
         curproc->pid, syscall_table[num].name, a, b, c, d, e, f, res);
#endif

  return res;
}

extern "C" void syscall_handle(int i, reg_t *regs) {
  regs[0] =
      do_syscall(regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6]);
}

