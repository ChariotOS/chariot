#include <arch.h>
#include <cpu.h>


void arch_disable_ints(void) {}
void arch_enable_ints(void) {}
void arch_relax(void) {}
void arch_halt() {}
void arch_mem_init(unsigned long mbd) {}
void arch_initialize_trapframe(bool userspace, reg_t *) {}
unsigned arch_trapframe_size(void) { return 0; }
void arch_dump_backtrace(void) {}
void arch_dispatch_function(void *func, long arg) {}
void arch_sigreturn(void) {}
void arch_flush_mmu(void) {}
void arch_save_fpu(struct thread &) {}
void arch_restore_fpu(struct thread &) {}
unsigned long arch_read_timestamp(void) { return 0; }

static struct processor_state dummy;
struct processor_state &cpu::current(void) {
  /* TODO: */
  return dummy;
}


void cpu::switch_vm(struct thread *thd) { /* TODO: nothin' */ }

void cpu::seginit(void *local) { /* TODO: nothin' */ }


/* TODO */
extern "C" void swtch(void) {}

/* TODO */
extern "C" void trapret(void) {}

/* TODO */
ref<mm::pagetable> mm::pagetable::create() { return nullptr; }


reg_t &arch_reg(int ind, reg_t *regs) { return regs[0]; /* TODO */ }


/* TODO: */
static mm::space kspace(0, 0x1000, nullptr);
mm::space &mm::space::kernel_space(void) {
  /* TODO: something real :) */
  return kspace;
}


int arch::irq::init(void) { return 0; /* TODO: */ }

void arch::irq::eoi(int i) {}
void arch::irq::enable(int num) {}
void arch::irq::disable(int num) {}
