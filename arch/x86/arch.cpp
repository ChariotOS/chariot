#include <arch.h>
#include <types.h>
#include <printk.h>

void arch_disable_ints(void) {
  asm volatile("cli");
}

void arch_enable_ints(void) {
  asm volatile("sti");
}

void arch_halt(void) {
  asm volatile("hlt");
}

void arch::invalidate_page(unsigned long addr) {
  asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

void arch_flush_mmu(void) {
  uint64_t tmpreg;

  asm volatile(
      "movq %%cr3, %0;  # flush TLB \n"
      "movq %0, %%cr3;              \n"
      : "=r"(tmpreg)::"memory");
}


unsigned long arch_read_timestamp(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}


void arch_relax(void) {
	asm("pause");
}
