#include <arch.h>
#include <cpu.h>
#include <riscv/arch.h>
#include <riscv/dis.h>
#include <riscv/plic.h>
#include <syscall.h>
#include <time.h>
#include <util.h>
#include <riscv/paging.h>

extern "C" void rv_enter_userspace(rv::regs *sp);

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
    auto *regs = (rv::regs *)tf;
    arch_enable_ints();

    if (thd->pid == thd->tid) {
      thd->setup_stack((reg_t *)tf);
    }

		/* Jumping to userspace */
    regs->status = read_csr(sstatus) & ~SSTATUS_SPP;
    regs->sepc = arch_reg(REG_PC, tf);
    rv_enter_userspace(regs);

    /* Jump into userspace :) */
    return;
  }

  sys::exit_proc(-1);
  while (1) {
  }
}



void arch_disable_ints(void) { rv::intr_off(); }
void arch_enable_ints(void) { rv::intr_on(); }

bool arch_irqs_enabled(void) { return rv::intr_enabled(); }


void arch_relax(void) {}

/* Simply wait for an interrupt :) */
void arch_halt() { asm volatile("wfi"); }

void arch_mem_init(unsigned long mbd) {}


void arch_initialize_trapframe(bool userspace, reg_t *r) {
  // auto *regs = (rv::regs*)r;
  // printk("pc: %p\n", regs->ra);
  // printk("sp: %p\n", regs->sp);
}


unsigned arch_trapframe_size(void) { return sizeof(rv::regs); }


void arch_dispatch_function(void *func, long arg) {}
void arch_sigreturn(void) {}
void arch_flush_mmu(void) {}
void arch_save_fpu(struct thread &) {}
void arch_restore_fpu(struct thread &) {}


unsigned long arch_read_timestamp(void) {
  rv::xsize_t x;
  asm volatile("csrr %0, time" : "=r"(x));
  return x;
}


struct rv::hart_state &rv::get_hstate(void) {
  rv::xsize_t sscratch;
  // asm volatile("csrr %0, sscratch" : "=r"(sscratch));
  return *(struct rv::hart_state *)p2v(rv::get_tp());
}

/*
 * Just offset into the cpu array with mhartid :^). I love this arch.
 * No need for bloated thread pointer bogus or nothin'
 */
struct processor_state &cpu::current(void) {
  return cpus[rv::get_hstate().hartid];
}


void cpu::switch_vm(struct thread *thd) {
  thd->proc.mm->switch_to();
	rv::sfence_vma();
}

void cpu::seginit(void *local) {
  auto &sc = rv::get_hstate();
  // printk(KERN_DEBUG "initialize hart %d\n", sc.hartid);
  auto &cpu = cpu::current();
  /* zero out the cpu structure. This might be bad idk... */
  memset(&cpu, 0, sizeof(struct processor_state));

  /* Forward this so other code can read it */
  cpu.cpunum = sc.hartid;
}


/* TODO */
extern "C" void trapret(void) {}




/*
 * TODO: we don't need some of these, as they are only called from within
 * x86. I won't implement them as a form of rebellion against complex
 * instruction sets
 */
int arch::irq::init(void) { return 0; /* TODO: */ }

void arch::irq::eoi(int i) {
  /* Forward to the PLIC */
  rv::plic::complete(i);
}
void arch::irq::enable(int num) { rv::plic::enable(num, 1); }
void arch::irq::disable(int num) { rv::plic::disable(num); }
