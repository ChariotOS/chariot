#include <arch.h>
#include <types.h>
#include <printk.h>
#include <cpu.h>
#include <syscall.h>
#include <time.h>

void arch_thread_create_callback() {
  auto thd = curthd;

  auto tf = thd->trap_frame;

  if (thd->proc.ring == RING_KERN) {
    using fn_t = int (*)(void *);
    auto fn = (fn_t)arch_reg(REG_PC, tf);
    arch_enable_ints();
    // run the kernel thread
    int res = fn(NULL);
    // exit the thread with the return code of the func
    sys::exit_thread(res);
  } else {
    if (time::stabilized()) {
      thd->last_start_utime_us = time::now_us();
    }
    if (thd->pid == thd->tid) {
      thd->setup_stack((reg_t *)tf);
    }
    arch_enable_ints();
    return;
  }

  sys::exit_proc(-1);
  while (1) {
  }
}

bool arch_irqs_enabled(void) {
  uint64_t rflags = 0;
  asm volatile("pushfq; popq %0" : "=a"(rflags));
  return (rflags & RFLAGS_IF) != 0;
}

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
