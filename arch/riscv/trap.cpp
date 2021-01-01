#include <printk.h>
#include <riscv/arch.h>
#include <riscv/plic.h>
#include <util.h>

struct ktrapframe {
  rv::xsize_t ra; /* x1: Return address */
  rv::xsize_t sp; /* x2: Stack pointer */
  rv::xsize_t gp; /* x3: Global pointer */
  rv::xsize_t tp; /* x4: Thread Pointer */
  rv::xsize_t t0; /* x5: Temp 0 */
  rv::xsize_t t1; /* x6: Temp 1 */
  rv::xsize_t t2; /* x7: Temp 2 */
  rv::xsize_t s0; /* x8: Saved register / Frame Pointer */
  rv::xsize_t s1; /* x9: Saved register */
  rv::xsize_t a0; /* Arguments, you get it :) */
  rv::xsize_t a1;
  rv::xsize_t a2;
  rv::xsize_t a3;
  rv::xsize_t a4;
  rv::xsize_t a5;
  rv::xsize_t a6;
  rv::xsize_t a7;
  rv::xsize_t s2; /* More Saved registers... */
  rv::xsize_t s3;
  rv::xsize_t s4;
  rv::xsize_t s5;
  rv::xsize_t s6;
  rv::xsize_t s7;
  rv::xsize_t s8;
  rv::xsize_t s9;
  rv::xsize_t s10;
  rv::xsize_t s11;
  rv::xsize_t t3; /* More temporaries */
  rv::xsize_t t4;
  rv::xsize_t t5;
  rv::xsize_t t6;
  /* Missing floating point registers in the kernel trap frame */
};


const char *ktrapframe_name[] = {
    "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5", "a6",
    "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",

};
void arch_dump_backtrace(void) { /* Nothing here for now... */
}



static void print_readable_reg(const char *name, rv::xsize_t value) {
  printk("%3s: ", name);

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


static void kernel_unhandled_trap(struct ktrapframe &tf, const char *type) {
  printk(KERN_ERROR "Unhandled trap '%s' on HART#%d\n", type, rv::hartid());
	printk(KERN_ERROR);
	print_readable_reg("PC", tf.ra);
	printk(", ");
	print_readable_reg("Bad Address", read_csr(sbadaddr));
	printk("\n");

  int p = 0;
  for (int i = 0; i < sizeof(struct ktrapframe) / sizeof(rv::xsize_t); i++) {
    print_readable_reg(ktrapframe_name[i], ((rv::xsize_t *)&tf)[i]);
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
extern "C" void kernel_trap(struct ktrapframe &tf) {
  int which_dev = 0;
  rv::xsize_t sepc = read_csr(sepc);
  rv::xsize_t sstatus = read_csr(sstatus);
  rv::xsize_t scause = read_csr(scause);

  if ((sstatus & SSTATUS_SPP) == 0) panic("kerneltrap: not from supervisor mode");
  if (rv::intr_enabled() != 0) panic("kerneltrap: interrupts enabled");

  int interrupt = (scause >> 63);
  int nr = scause & ~(1llu << 63);
  if (interrupt) {
    /* Supervisor software interrupt (from machine mode) */
    if (nr == 1) {
      // acknowledge the software interrupt by clearing
      // the SSIP bit in sip.
      write_csr(sip, read_csr(sip) & ~2);
    }
    /* Supervisor External Interrupt */
    if (nr == 9) {
      int irq = rv::plic::claim();
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

extern "C" void machine_trap(struct ktrapframe &tf) {}
