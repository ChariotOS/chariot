#include <asm.h>
#include <cpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <map.h>
#include <mem.h>
#include <mmap_flags.h>
#include <module.h>
#include <phys.h>
#include <syscall.h>
#include <util.h>
#include <vga.h>

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


static uint64_t do_syscall(long num, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
  if (num & !0xFF) return -1;

  if (syscall_table[num].handler == NULL) {
    return -1;
  }
  curthd->stats.syscall_count++;

  auto *func = (uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))syscall_table[num].handler;
  auto res = func(a, b, c, d, e, f);

  return res;
}

extern "C" void syscall_handle(int i, reg_t *regs, void* data) {
  regs[0] = do_syscall(regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6]);
}


static void syscall_init(void) {
#undef __SYSCALL
#define __SYSCALL(num, name, args...) set_syscall(#name, num, (void *)sys::name);
#include <syscalls.inc>
#undef __SYSCALL
}


module_init("syscall", syscall_init);
