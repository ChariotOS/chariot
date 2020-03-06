#include <cpu.h>
#include <mem.h>
#include <phys.h>

#include "smp.h"

static __thread cpu_t *s_current = nullptr;

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
static inline void ltr(u16 sel) { asm volatile("ltr %0" : : "r"(sel)); }

void cpu::seginit(void *local) {
#ifdef __x86_64__
  if (local == nullptr) local = p2v(phys::alloc());

  // make sure the local information segment is zeroed
  memset(local, 0, PGSIZE);

  auto *gdt = (u64 *)local;
  auto *tss = (u32 *)(((char *)local) + 1024);
  tss[16] = 0x00680000;  // IO Map Base = End of TSS

  tss[0x64] |= (0x64 * sizeof(u32)) << 16;

  wrmsr(0xC0000100, ((u64)local) + (PGSIZE / 2));

  // zero out the CPU
  cpu_t *c = &cpus[cpunum++];
  memset(c, 0, sizeof(*c));
  c->local = local;

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

  lgdt((void *)gdt, 8 * sizeof(u64));

  ltr(SEG_TSS << 3);

#endif
}

static void tss_set_rsp(u32 *tss, u32 n, u64 rsp) {
#ifdef __x86_64__
  tss[n * 2 + 1] = rsp;
  tss[n * 2 + 2] = rsp >> 32;
#endif
}

void cpu::switch_vm(struct thread *thd) {
  cpu::pushcli();
  auto c = current();
  auto tss = (u32 *)(((char *)c.local) + 1024);

  switch (thd->proc.ring) {
    case RING_KERN:
      tss_set_rsp(tss, 0, 0);
      break;

    case RING_USER:
      tss_set_rsp(tss, 0, (u64)thd->stack + thd->stack_size + 8);
      break;

    default:
      panic("unknown ring %d in cpu::switch_vm\n", thd->proc.ring);
      break;
  }

  thd->proc.mm->switch_to();

  cpu::popcli();
}

cpu_t &cpu::current() {
  if (s_current == NULL) {
    int ind = smp::cpunum();

    s_current = &cpus[ind];
  }

  return *s_current;
}
