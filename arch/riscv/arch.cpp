#include <arch.h>
#include <cpu.h>
#include <riscv/arch.h>
#include <riscv/sbi.h>
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




void arch_flush_mmu(void) {}


extern "C" void __rv_save_fpu(void *);
extern "C" void __rv_load_fpu(void *);

void arch_save_fpu(Thread &thd) { __rv_save_fpu(thd.fpu.state); }
void arch_restore_fpu(Thread &thd) { __rv_load_fpu(thd.fpu.state); }


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
cpu::Core &cpu::current(void) {
  return *rv::get_hstate().cpu;
}


void cpu::switch_vm(ck::ref<Thread> thd) {
  thd->proc.mm->switch_to();
  rv::sfence_vma();
}


void cpu::seginit(cpu::Core *cpu, void *local) {
  auto &sc = rv::get_hstate();
  /* Forward this so other code can read it */
  cpu->id = sc.hartid;
	cpu->active = true;
	/* Register the CPU with the kernel */
	cpu::add(cpu);
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


void arch_deliver_xcall(int core) {
	unsigned long mask = (core == -1) ? ~0 : (1 << core);
	printk_nolock("deliver xcall: %d %p\n", core, mask);
	sbi_send_ipis(&mask);
}
