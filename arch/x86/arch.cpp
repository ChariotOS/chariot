#include <arch.h>
#include <types.h>
#include <printk.h>

void arch::cli(void) {
  asm volatile("cli");
}

void arch::sti(void) {
  asm volatile("sti");
}

void arch::halt(void) {
  asm volatile("hlt");
}

void arch::invalidate_page(unsigned long addr) {
  asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

void arch::flush_tlb(void) {
  uint64_t tmpreg;

  asm volatile(
      "movq %%cr3, %0;  # flush TLB \n"
      "movq %0, %%cr3;              \n"
      : "=r"(tmpreg)::"memory");
}


unsigned long arch::read_timestamp(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
  uint64_t ret;
  asm volatile("pushfq; popq %0" : "=a"(ret));
  return ret;
}


void arch::relax(void) {
	asm("pause");
}
