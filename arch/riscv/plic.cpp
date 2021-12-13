#include <riscv/plic.h>
#include <riscv/arch.h>
#include <riscv/memlayout.h>
#include <printk.h>
#include <util.h>
#include <cpu.h>
int boot_hart = -1;


#define LOG(...) PFXLOG(BLU "riscv,plic", __VA_ARGS__)

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

  /*
printk("plic on %d base = %p\n", hart, enable_base);
printk("hart=%d\n", hart);
printk("irq=%p\n", hwirq);
printk("reg=%p\n", reg);
printk("mask=%08x\n", hwirq_mask);
  */

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


  for (int i = 0; i < 0x1000 / 4; i++) {
    MREG(PLIC + i * 4) = 7;
  }

  (&PLIC_SENABLE(hart))[0] = 0;
  (&PLIC_SENABLE(hart))[1] = 0;
  (&PLIC_SENABLE(hart))[2] = 0;
}

uint32_t rv::plic::pending(void) {
  /*
off_t base = PLIC + 0x1000;
for (int i = 0; i < 3; i++) {
uint32_t &reg = MREG(base + i * 4);
for (int b = 0; b < 32; b++) {
printk("%d", (reg >> b) & 0b1);
}
printk(" ");
}
printk("\n");
  */

  return PLIC_PENDING;
}


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
  return;

  off_t enable_base = PLIC + ENABLE_BASE + rv::hartid() * ENABLE_PER_HART;
  for (int i = 0; i < 3; i++) {
    uint32_t &reg = MREG(enable_base + i * 4);
    for (int b = 0; b < 32; b++) {
      printk("%d", (reg >> b) & 0b1);
    }
    printk(" ");
  }
  printk("\n");
}

void rv::plic::disable(int hwirq) { plic_toggle(rv::hartid(), hwirq, 0, false); }
