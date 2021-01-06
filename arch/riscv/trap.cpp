#include <cpu.h>
#include <printk.h>
#include <riscv/arch.h>
#include <riscv/plic.h>
#include <sched.h>
#include <time.h>
#include <util.h>



reg_t &arch_reg(int ind, reg_t *r) {
  auto *tf = (struct rv::regs *)r;
  switch (ind) {
    case REG_PC:
      return tf->ra;

    case REG_SP:
      return tf->sp;

    case REG_BP:
      return tf->s0;

    case REG_ARG0:
      return tf->a0;
    case REG_ARG1:
      return tf->a1;
  }
  panic("INVALID arch_reg() CALL %d\n", ind);
  while (1) {
  }
}

const char *regs_name[] = {
    "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5", "a6",
    "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "sepc",
};


void arch_dump_backtrace(void) { /* Nothing here for now... */
}



static void print_readable_reg(const char *name, rv::xsize_t value) {
  printk("%4s: ", name);

  char buf[sizeof(value) * 2 + 1];
  snprintk(buf, sizeof(buf), "%p", value);

  bool seen = false;
  for (int i = 0; i < sizeof(value) * 2; i++) {
    const char *p = buf + i;
    if (*p == '0' && !seen) {
      set_color(C_GRAY);
    } else {
      if (!seen) set_color(C_RESET);
      seen = true;

      /* If we are on an even boundary */
      if ((i & 1) == 0) {
        /* This is inefficient, but it lets me see ascii chars in the reg dump */
        char buf[3];
        buf[0] = p[0];
        buf[1] = p[1];
        buf[2] = 0;

        uint8_t c = 0;
        sscanf(buf, "%02x", &c);
        set_color_for(c, C_RESET);
      }
    }
    printk("%c", *p);
  }

  set_color(C_RESET);
}


static void kernel_unhandled_trap(struct rv::regs &tf, const char *type) {
  printk(KERN_ERROR "Unhandled trap '%s' on HART#%d\n", type, rv::hartid());
  printk(KERN_ERROR);
  print_readable_reg("SEPC", tf.sepc);
  printk(", ");
  print_readable_reg("Bad Address", read_csr(sbadaddr));

  printk(", ");
  print_readable_reg("val", read_csr(stval));
  printk("\n");

  printk(KERN_ERROR);
  print_readable_reg("sscratch", read_csr(sscratch));
  printk("\n");

  int p = 0;
  for (int i = 0; i < sizeof(struct rv::regs) / sizeof(rv::xsize_t); i++) {
    if (p == 0) printk(KERN_ERROR);

    print_readable_reg(regs_name[i], ((rv::xsize_t *)&tf)[i]);
    p++;
    if (p >= 4) {
      printk("\n");
      p = 0;
    } else {
      printk(" ");
    }
  }

  if (p != 0) printk("\n");

  panic("Halting hart!\n");
}


/* Supervisor trap function */
extern "C" void kernel_trap(struct rv::regs &tf) {
  int which_dev = 0;
  rv::xsize_t sepc = read_csr(sepc);
  rv::xsize_t sstatus = read_csr(sstatus);
  rv::xsize_t scause = read_csr(scause);

  if ((sstatus & SSTATUS_SPP) == 0) panic("kerneltrap: not from supervisor mode");
  if (rv::intr_enabled() != 0) panic("kerneltrap: interrupts enabled");



#ifdef CONFIG_64BIT
  int interrupt = (scause >> 63);
#else
  int interrupt = (scause >> 31);
#endif
  int nr = scause & ~(1llu << 63);
  if (interrupt) {
    /* Supervisor software interrupt (from machine mode) */
    if (nr == 1) {
      auto &cpu = cpu::current();
      uint64_t now = arch_read_timestamp();
      cpu.kstat.tsc_per_tick = now - cpu.kstat.last_tick_tsc;
      cpu.kstat.last_tick_tsc = now;
      cpu.kstat.ticks++;


      /* Does anyone need to wake up? */
      if (check_wakeups()) {
        /* TODO */
      }

      // acknowledge the software interrupt by clearing
      // the SSIP bit in sip.
      write_csr(sip, read_csr(sip) & ~2);

      sched::handle_tick(cpu.kstat.ticks);
    }

    /* Supervisor External Interrupt */
    if (nr == 9) {
      /* First, we claim the irq from the PLIC */
      int irq = rv::plic::claim();
      /* *then* enable interrupts, so the irq handler can have interrupts.
       * This is a major design problem in chariot, and I gotta figure out
       * if I can just have them disabled in irq handlers, and defer to worker
       * threads outside of irq context generally.
       */
      rv::intr_on();

      // printk("irq: 0x%d\n", irq);
      irq::dispatch(irq, NULL /* Hmm, not sure what to do with regs */);
      rv::plic::complete(irq);
    }
  } else {
    switch (nr) {
      case 0:
        kernel_unhandled_trap(tf, "Instruction address misaligned");
        break;

      case 1:
        kernel_unhandled_trap(tf, "Instruction access fault");
        break;

      case 2:
        kernel_unhandled_trap(tf, "Illegal Instruction");
        break;

      case 3:
        kernel_unhandled_trap(tf, "Breakpoint");
        break;

      case 4: /* Shouldn't get this.. I hope */
        kernel_unhandled_trap(tf, "Reserved");
        break;

      case 5:
        kernel_unhandled_trap(tf, "Load access fault");
        break;

      case 6:
        kernel_unhandled_trap(tf, "AMO address misaligned");
        break;

      case 7:
        kernel_unhandled_trap(tf, "Store/AMO access fault");
        break;

      case 8:
        kernel_unhandled_trap(tf, "Environment call");
        break;

      default:
        kernel_unhandled_trap(tf, ">=9 Reserved");
        break;
    }
  }

  /* restore these regs in case other code causes traps */
  write_csr(sepc, sepc);
  write_csr(sstatus, sstatus);
}

extern "C" void machine_trap(struct rv::regs &tf) {}
