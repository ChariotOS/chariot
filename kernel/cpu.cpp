#include <arch.h>
#include <asm.h>
#include <cpu.h>
#include <idt.h>
#include <mem.h>
#include <phys.h>
#include <printk.h>
#include <syscall.h>
#include <types.h>



struct processor_state cpus[CONFIG_MAX_CPUS];
int cpunum = 0;

int cpu::nproc(void) { return cpunum; }

struct processor_state *cpu::get() {
  return &cpu::current();
}

struct process *cpu::proc(void) {
  if (curthd == NULL) return NULL;
  return &curthd->proc;
}

bool cpu::in_thread(void) { return (bool)thread(); }

struct thread *cpu::thread() {
  return current().current_thread;
}

extern "C" u64 get_sp(void);



int sys::get_core_usage(unsigned int core, struct chariot_core_usage *usage) {
  if (core > CONFIG_MAX_CPUS) return -1;
  if (core > cpu::nproc()) return -1;

  auto &c = cpus[core];

  if (!VALIDATE_WR(usage, sizeof(struct chariot_core_usage))) {
    return -EINVAL;
  }


  usage->user_ticks = c.kstat.user_ticks;
  usage->kernel_ticks = c.kstat.kernel_ticks;
  usage->idle_ticks = c.kstat.idle_ticks;
  usage->ticks_per_second = c.ticks_per_second;

  return 0;
}

int sys::get_nproc(void) { return cpu::nproc(); }

extern "C" int get_errno(void) { return curthd->kerrno; }
