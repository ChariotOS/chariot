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
  auto start_tick = c.kstat.ticks;

  arch::sti();

  // spin while recording
  while (1) {
    if (c.kstat.ticks - start_tick > rec_ms) {
      break;
    }
		arch::relax();
  }

  double cycles = arch::read_timestamp() - start_cycle;

  arch::cli();

  double hz = (cycles / rec_ms) * 1000.0;
  c.speed_khz = hz / 1000;
  KINFO("%zu khz\n", c.speed_khz);
}

