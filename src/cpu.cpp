#include <asm.h>
#include <cpu.h>
#include <idt.h>
#include <mem.h>
#include <paging.h>
#include <phys.h>
#include <printk.h>
#include <types.h>

// 16 CPU structures where each cpu has one
static cpu_t cpus[16];
static int cpunum = 0;
static __thread cpu_t *s_current = nullptr;

cpu_t &cpu::current() {
  // TODO: multi processor stuff
  if (s_current == nullptr)
    panic("called cpu::current without initializing the cpu first\n");

  return *s_current;
}

cpu_t *cpu::get() { return s_current; }

process &cpu::proc(void) {
  return thd().proc();
}

bool cpu::in_thread(void) {
  return current().current_thread != nullptr;
}

thread &cpu::thd() {
  auto *t = current().current_thread;
  assert(t != nullptr);
  return *t;
}

extern "C" void wrmsr(u32 msr, u64 val);

struct gdt_desc64 {
  uint16_t limit;
  uint64_t base;
} __packed;
static inline void lgdt(void *data, int size) {
  gdt_desc64 gdt;
  gdt.limit = size - 1;
  gdt.base = (u64)data;

  asm volatile("lgdt %0" ::"m"(gdt));
}

extern "C" u64 get_sp(void);

static inline void ltr(u16 sel) { asm volatile("ltr %0" : : "r"(sel)); }

void cpu::seginit(void *local) {
  if (local == nullptr) {
    local = p2v(phys::alloc());
  }

  // make sure the local information segment is zeroed
  memset(local, 0, PGSIZE);

  auto *gdt = (u64 *)local;
  auto *tss = (u32 *)(((char *)local) + 1024);
  tss[16] = 0x00680000;  // IO Map Base = End of TSS

  wrmsr(0xC0000100, ((u64)local) + (PGSIZE / 2));

  cpu_t *c = &cpus[cpunum++];
  c->local = local;

  s_current = c;

  auto addr = (u64)tss;
  gdt[0] = 0x0000000000000000;
  gdt[SEG_KCODE] = 0x002098000000FFFF;  // Code, DPL=0, R/X
  gdt[SEG_UCODE] = 0x0020F8000000FFFF;  // Code, DPL=3, R/X
  gdt[SEG_KDATA] = 0x000092000000FFFF;  // Data, DPL=0, W
  gdt[SEG_KCPU] = 0x000000000000FFFF;   // unused
  gdt[SEG_UDATA] = 0x0000F2000000FFFF;  // Data, DPL=3, W
  gdt[SEG_TSS + 0] = (0x0067) | ((addr & 0xFFFFFF) << 16) | (0x00E9LL << 40) |
                     (((addr >> 24) & 0xFF) << 56);
  gdt[SEG_TSS + 1] = (addr >> 32);

  lgdt((void *)gdt, 5 * sizeof(u64));

  // ltr(SEG_TSS << 3);
}

void cpu::calc_speed_khz(void) {
  auto &c = current();
  c.speed_khz = 0;

  u64 rec_ms = 500;
  auto start_cycle = rdtsc();

  auto start_tick = c.ticks;

  sti();

  // spin while recording
  while (1) {
    // idk why this needs to be here... probably to uncache the c.ticks? idk
    // dude
    printk("");
    if (c.ticks - start_tick > rec_ms) {
      break;
    }
  }

  double cycles = rdtsc() - start_cycle;

  cli();

  double hz = (cycles / rec_ms) * 1000.0;
  c.speed_khz = hz / 1000;
  // printk("%zu khz\n", c.speed_khz);
}

// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.

void cpu::pushcli(void) {
  // printk("%p\n", s_current);
  // if (s_current == nullptr) return;
  int eflags;

  eflags = readeflags();
  cli();
  if (current().ncli++ == 0) current().intena = eflags & FL_IF;
}

void cpu::popcli(void) {
  // if (s_current == nullptr) return;
  if (readeflags() & FL_IF) panic("popcli - interruptible");
  if (--current().ncli < 0) panic("popcli");
  if (current().ncli == 0 && current().intena) sti();
}
