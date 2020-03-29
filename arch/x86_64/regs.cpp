#include <arch.h>
#include <printk.h>
#include <cpu.h>

reg_t &arch::reg(int ind, reg_t *r) {
#ifdef __x86_64__
  switch (ind) {
    case REG_PC:
      return r[17];

    case REG_SP:
      return r[20];

    case REG_BP:
      return r[9];
  }
#endif

  panic("regs: invalid ind %d\n", ind);
  while (1) {}
}

unsigned arch::trapframe_size() {
#ifdef __x86_64__
  // there are 22 registers stored in the x86_64 trapframe
  return sizeof(reg_t) * 22;
#endif

  panic("trapframe_size: unimplemneted\n");
  return 0;
}

void arch::initialize_trapframe(bool userspace, reg_t *r) {

#ifdef __x86_64__
  if (userspace) {
    r[18 /* CS */] = (SEG_UCODE << 3) | DPL_USER;
    r[21 /* DS */] = (SEG_UDATA << 3) | DPL_USER;
    r[19 /* FL */] = FL_IF ;
  } else {
    r[18 /* CS */] = (SEG_UCODE << 3) | DPL_KERN;
    r[21 /* DS */] = (SEG_UDATA << 3) | DPL_KERN;
    r[19 /* FL */] = FL_IF | readeflags();
  }
  return;
#endif

  panic("initialize_trapframe: unimplemneted\n");
}



