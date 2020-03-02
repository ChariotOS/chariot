#include <arch.h>
#include <asm.h>
#include <cpu.h>
#include <idt.h>
#include <mem.h>
#include <phys.h>
#include <printk.h>
#include <types.h>

// 16 CPU structures where each cpu has one
cpu_t cpus[16];
int cpunum = 0;

cpu_t *cpu::get() { return &cpu::current(); }

struct process *cpu::proc(void) {
  if (curthd == NULL) return NULL;
  return &curthd->proc;
}

bool cpu::in_thread(void) { return (bool)thread(); }

struct thread *cpu::thread() {
  return current().current_thread;
}

extern "C" u64 get_sp(void);

void cpu::calc_speed_khz(void) {
  auto &c = current();
  c.speed_khz = 0;
  u64 rec_ms = 500;
  auto start_cycle = arch::read_timestamp();
  auto start_tick = c.ticks;

  arch::sti();

  // spin while recording
  while (1) {
    if (c.ticks - start_tick > rec_ms) {
      break;
    }
    asm("pause");
  }

  double cycles = arch::read_timestamp() - start_cycle;
  pushcli();

  arch::cli();

  double hz = (cycles / rec_ms) * 1000.0;
  c.speed_khz = hz / 1000;
  printk("%zu khz\n", c.speed_khz);
}

// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.
void cpu::pushcli(void) {
  arch::cli();
  current().ncli++;
}

void cpu::popcli(void) {
  if (readeflags() & FL_IF) panic("popcli - interruptible");
  if (--current().ncli < 0) panic("popcli");
  if (current().ncli == 0) arch::sti();
}

void cpu::preempt_enable(void) {
  auto *c = cpu::get();
  if (c != NULL) {
    __sync_fetch_and_add(&c->preemption_depth, 1);
  }
}
void cpu::preempt_disable(void) {
  auto *c = cpu::get();
  if (c != NULL) {
    __sync_fetch_and_sub(&c->preemption_depth, 1);
  }
}

void cpu::preempt_reset(void) {
  auto *c = cpu::get();
  if (c != NULL) {
    c->preemption_depth = 0;
  }
}

int cpu::preempt_disabled(void) {
  auto *c = cpu::get();
  if (c != NULL) {
    return __sync_fetch_and_add(&c->preemption_depth, 0) > 0;
  } else
    return 1;
}
