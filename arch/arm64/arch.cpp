#include <arch.h>
#include <cpu.h>
#include <arm64/arch.h>
#include <mm.h>

reg_t &arch_reg(int ind, reg_t *r) {
  auto *tf = (struct arm64::regs *)r;
  switch (ind) {
    case REG_PC:
      return tf->pc;

    case REG_SP:
      return tf->sp;

      /*
case REG_BP:
return tf->s0;
      */

    case REG_ARG0:
      return tf->r0;
    case REG_ARG1:
      return tf->r1;
  }
  panic("INVALID arch_reg() CALL %d\n", ind);
  while (1) {
  }
}

void arch_dump_backtrace(void) {}


extern "C" void context_switch(void) {}

void arch_disable_ints(void) {
  /* TODO: look into the meaning of this */
  asm("MSR DAIFSET, #2" :::);
}
void arch_enable_ints(void) { asm("MSR DAIFCLR, #2" :::); }

bool arch_irqs_enabled(void) {
  uint32_t val;

  asm("MRS %[v], DAIF" : [v] "=r"(val)::);

  return !(val & DIS_INT);
}


void arch_relax(void) {}

/* Simply wait for an interrupt :) */
void arch_halt() { asm volatile("wfi"); }

void arch_mem_init(unsigned long mbd) {}


void arch_initialize_trapframe(bool userspace, reg_t *r) {
  // auto *regs = (rv::regs*)r;
  // printf("pc: %p\n", regs->ra);
  // printf("sp: %p\n", regs->sp);
}


unsigned arch_trapframe_size(void) { return sizeof(arm64::regs); }


void arch_dispatch_function(void *func, long arg) {}
void arch_sigreturn(void) {}
void arch_flush_mmu(void) {}
void arch_save_fpu(struct thread &) {}
void arch_restore_fpu(struct thread &) {}

unsigned long arch_read_timestamp(void) {
  /* TODO figure out about arm's time model */
  arm64::reg_t x = 0;
  return x;
}

unsigned long arch_seconds_since_boot(void) {
  /* TODO: */
  return 0;
}


/* TODO */
extern "C" void trapret(void) {}

/* TODO */
ck::ref<mm::PageTable> mm::PageTable::create() { return nullptr; }

/* TODO: */
static mm::AddressSpace kspace(0, 0x1000, nullptr);
mm::AddressSpace &mm::AddressSpace::kernel_space(void) {
  /* TODO: something real :) */
  return kspace;
}


/*
 * TODO: we don't need some of these, as they are only called from within
 * x86. I won't implement them as a form of rebellion against complex
 * instruction sets
 */
int arch::irq::init(void) { return 0; /* TODO: */ }

void arch::irq::eoi(int i) { /* TODO */
}
void arch::irq::enable(int num) { /* TODO */
}
void arch::irq::disable(int num) { /* TODO */
}




static inline int get_cpu_id(void) {
  /* TODO */
  return 0;
}

/*
 * Just offset into the cpu array with mhartid :^). I love this arch.
 * No need for bloated thread pointer bogus or nothin'
 */
cpu::Core &cpu::current(void) { return *cpu::get(get_cpu_id()); }


void cpu::switch_vm(ck::ref<Thread> thd) { /* TODO: nothin' */
}

void cpu::seginit(cpu::Core *c, void *local) {
  // printf(KERN_DEBUG "initialize hart %d\n", sc.hartid);
  auto &cpu = cpu::current();
  /* zero out the cpu structure. This might be bad idk... */
  memset(&cpu, 0, sizeof(cpu::Core));

  /* Forward this so other code can read it */
  cpu.id = get_cpu_id();
}


void arch_sigreturn(void *ucontext) {}

void arch_save_fpu(struct Thread &) {}
void arch_restore_fpu(struct Thread &) {}

void arch_thread_create_callback(void) {}

int arch_generate_backtrace(off_t virt_bp, off_t *bt, off_t sz) { return 0; }

void arch_deliver_xcall(int core) {}
