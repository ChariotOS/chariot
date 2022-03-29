#include <riscv/plic.h>
#include <riscv/arch.h>
#include <riscv/memlayout.h>
#include <printf.h>
#include <util.h>
#include <cpu.h>
int boot_hart = -1;


#define LOG(...) PFXLOG(BLU "riscv,plic", __VA_ARGS__)


// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC ((rv::xsize_t)p2v(0x0c000000L))
#define PLIC_PRIORITY MREG(PLIC + 0x0)
#define PLIC_PENDING MREG(PLIC + 0x1000)
#define PLIC_MENABLE(hart) MREG(PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) MREG(PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) MREG(PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) MREG(PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) MREG(PLI + 0x201004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) MREG(PLIC + 0x200004 + (hart)*0x2000)

/*
 * Each interrupt source has a priority register associated with it.
 * We always hardwire it to one in Linux.
 */
#define PRIORITY_BASE 0
#define PRIORITY_PER_ID 4

/*
 * Each hart context has a vector of interrupt enable bits associated with it.
 * There's one bit for each interrupt source.
 */
#define ENABLE_BASE 0x2000
#define ENABLE_PER_HART 0x100


static void plic_toggle(int hart, int hwirq, int priority, bool enable) {
  off_t enable_base = PLIC + ENABLE_BASE + hart * ENABLE_PER_HART;
  uint32_t &reg = MREG(enable_base + (hwirq / 32) * 4);
  uint32_t hwirq_mask = 1 << (hwirq % 32);
  MREG(PLIC + 4 * hwirq) = 7;
  PLIC_SPRIORITY(hart) = 0;

  if (enable) {
    reg = reg | hwirq_mask;
  } else {
    reg = reg & ~hwirq_mask;
  }
}


void rv::plic::hart_init(void) {
  int hart = rv::hartid();
  LOG("Initializing on hart#%d\n", hart);

  for (int i = 0; i < 0x1000 / 4; i++)
    MREG(PLIC + i * 4) = 7;

  (&PLIC_SENABLE(hart))[0] = 0;
  (&PLIC_SENABLE(hart))[1] = 0;
  (&PLIC_SENABLE(hart))[2] = 0;
}

uint32_t rv::plic::pending(void) { return PLIC_PENDING; }


int rv::plic::claim(void) {
  int hart = rv::hartid();
  int irq = PLIC_SCLAIM(hart);
  return irq;
}

void rv::plic::complete(int irq) {
  int hart = rv::hartid();
  PLIC_SCLAIM(hart) = irq;
}


void rv::plic::enable(int hwirq, int priority) {
  LOG("enable hwirq=%d, priority=%d\n", hwirq, priority);
  plic_toggle(rv::hartid(), hwirq, priority, true);
}

void rv::plic::disable(int hwirq) { plic_toggle(rv::hartid(), hwirq, 0, false); }
