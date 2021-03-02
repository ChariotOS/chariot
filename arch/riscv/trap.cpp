#include <cpu.h>
#include <printk.h>
#include <riscv/arch.h>
#include <riscv/paging.h>
#include <riscv/plic.h>
#include <sched.h>
#include <time.h>
#include <util.h>


#include <riscv/sbi.h>

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L /* Not virtual, as this is used in machine modew */
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8 * (hartid))
#define CLINT_MTIME (CLINT + 0xBFF8)  // cycles since boot.


reg_t &arch_reg(int ind, reg_t *r) {
  auto *tf = (struct rv::regs *)r;
  switch (ind) {
    case REG_PC:
      return tf->sepc;

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

const char *regs_name[] = {"ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",  "a1",
                           "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",  "s6",
                           "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "sepc"};


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


#define NBITS(N) ((1LL << (N)) - 1)

static void kernel_unhandled_trap(struct rv::regs &tf, const char *type) {
  rv::xsize_t bad_addr = tf.tval;
  printk(
      "==========================================================================================="
      "\n");
  printk("Unhandled trap '%s' on HART#%d\n", type, rv::hartid());
  print_readable_reg("SEPC", tf.sepc);
  printk(", ");
  print_readable_reg("Bad Address", bad_addr);
  printk(", ");
  print_readable_reg("satp", read_csr(satp));
  printk("\n");


  /* Print the VM walk of the address */
  printk(" VM walk (b): ");
  int mask = (1LLU << VM_PART_BITS) - 1;
  int awidth = (VM_PART_BITS * VM_PART_NUM) + 12;
  for (int i = VM_PART_NUM - 1; i >= 0; i--) {
    printk("%0*b ", VM_PART_BITS, (bad_addr >> (VM_PART_BITS * i + 12)) & mask);
  }
  printk("+ %012b", bad_addr & 0xFFF);
  printk("\n");


  printk("         (d): ");
  for (int i = VM_PART_NUM - 1; i >= 0; i--) {
    printk("% *d ", VM_PART_BITS, (bad_addr >> (VM_PART_BITS * i + 12)) & mask);
  }
  printk("+ %12d", bad_addr & 0xFFF);
  printk("\n");

  print_readable_reg("val", tf.tval);
  printk(", ");
  print_readable_reg("scr", tf.scratch);
  printk("\n");

  int p = 0;
  for (int i = 0; i < 32; i++) {
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

  if (cpu::in_thread()) {
    printk("\n");
    printk("Address Space:\n");
    auto proc = curproc;
    proc->mm->dump();
  }

  printk(
      "==========================================================================================="
      "\n");
  panic("Halting hart!\n");
}


static void pgfault_trap(struct rv::regs &tf, const char *type_name, int err) {
  auto addr = tf.tval;
  auto page = addr >> 12;


  auto proc = curproc;

  // Now that we have the addr, we can re-enable interrupts
  // (further irqs might corrupt the csr)
  arch_enable_ints();

  if (curproc == NULL) {
    KERR("not in a proc while pagefaulting (rip=%p, addr=%p)\n", tf.sepc, addr);
    // arch_dump_backtrace();
    // lookup the kernel proc if we aren't in one!
    proc = sched::proc::kproc();
  }


  int res = proc->mm->pagefault(addr, err);

  if (res == -1) {
    /* send to the current thread and return (handle at the bottom of kernel_vec) */
    curthd->send_signal(SIGSEGV);
    return;
  }

  rv::sfence_vma(addr);
}



extern uint64_t do_syscall(long num, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e,
                           uint64_t f);

void rv_handle_syscall(rv::regs &tf) {
  arch_enable_ints();
  tf.sepc += 4;

  // printk(KERN_INFO "do syscall: %d\n", tf.a0);
  tf.a0 = do_syscall(tf.a0, tf.a1, tf.a2, tf.a3, tf.a4, tf.a5, tf.a6);
  // printk(KERN_INFO " res = %p\n", tf.a0);
}

/* Supervisor trap function */
extern "C" void kernel_trap(struct rv::regs &tf) {
  bool from_userspace = false;

	auto *thd = curthd;

  if ((tf.status & SSTATUS_SPP) == 0) {
    from_userspace = true;
    // printk("kerneltrap: not from supervisor mode: pc=%p", tf.sepc);
  }

  int which_dev = 0;

  reg_t *old_trapframe = NULL;
  if (cpu::in_thread()) {
    old_trapframe = thd->trap_frame;
    thd->trap_frame = (reg_t *)&tf;
    if (from_userspace) {
      thd->userspace_sp = tf.sp;
    }
  }


  /* The previous stack pointer located in the scratch */
  rv::xsize_t previous_kernel_stack = rv::get_hstate().kernel_sp;

  if (cpu::in_thread()) {
    if (thd->stacks.size() != 1) {
      printk_nolock(KERN_DEBUG "previous kernel stack: %p\n");
      for (auto &stk : thd->stacks) {
        printk_nolock(KERN_DEBUG "   %d %p\n", stk.size, stk.start);
      }
    }
  }

  if (rv::intr_enabled() != 0) panic("kerneltrap: interrupts enabled");

#ifdef CONFIG_64BIT
  int interrupt = (tf.cause >> 63);
#else
  int interrupt = (tf.cause >> 31);
#endif
  int nr = tf.cause & ~(1llu << 63);
  if (interrupt) {
    /* Supervisor software interrupt (from machine mode) */
    if (nr == 1) {
#ifndef CONFIG_SBI
      /* TODO: move this to the  */
#endif
    } else if (nr == 5) {
      auto &cpu = cpu::current();
      uint64_t now = arch_read_timestamp();
      cpu.kstat.tsc_per_tick = now - cpu.kstat.last_tick_tsc;
      cpu.kstat.last_tick_tsc = now;
      cpu.kstat.ticks++;

#ifdef CONFIG_SBI
      /* TODO: write the next time */
      sbi_set_timer(rv::get_time() + TICK_INTERVAL);
#else
      // acknowledge the software interrupt by clearing
      // the SSIP bit in sip.
      // TODO: move this to the right place :)
      write_csr(sip, read_csr(sip) & ~2);
#endif

      arch_enable_ints();

      sched::handle_tick(cpu.kstat.ticks);
    } else if (nr == 9) {
      /* Supervisor External Interrupt */
      /* First, we claim the irq from the PLIC */
      int irq = rv::plic::claim();
      /* *then* enable interrupts, so the irq handler can have interrupts.
       * This is a major design problem in chariot, and I gotta figure out
       * if I can just have them disabled in irq handlers, and defer to worker
       * threads outside of irq context generally.
       */
      // rv::intr_on();

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
        kernel_unhandled_trap(tf, "Instruction Access Fault");
        break;

      case 2:
        kernel_unhandled_trap(tf, "Illegal Instruction");
        break;

      case 3:
        kernel_unhandled_trap(tf, "Breakpoint");
        break;

      case 4:
        kernel_unhandled_trap(tf, "Load Address Misaligned");
        break;

      case 5:
        kernel_unhandled_trap(tf, "Load Access Fault");
        break;

      case 6:
        kernel_unhandled_trap(tf, "Store/AMO Address Misaligned");
        break;

      case 7:
        kernel_unhandled_trap(tf, "Store/AMO Access Fault");
        break;

      case 8:
        rv_handle_syscall(tf);
        break;

      case 9:
        kernel_unhandled_trap(tf, "Environment Call from S-Mode");
        break;

      case 11:
        kernel_unhandled_trap(tf, "Environment Call from M-Mode");
        break;

      case 12:
        pgfault_trap(tf, "Instruction Page Fault", FAULT_EXEC);
        break;

      case 13:
        pgfault_trap(tf, "Load Page Fault", FAULT_READ);
        break;

      case 15:
        pgfault_trap(tf, "Store/AMO Page Fault", FAULT_WRITE);
        break;

      default:
        kernel_unhandled_trap(tf, "Reserved");
        break;
    }
  }


  sched::before_iret(from_userspace);

  int sig = 0;
  void *handler = NULL;
  if (sched::claim_next_signal(sig, handler) != -1) {

		uint64_t sp = tf.sp;

		sp -= sizeof(tf);
		auto *uctx = (rv::regs *)sp;
		/* save the old context to the user stack */
		if (!VALIDATE_RDWR(uctx, sizeof(*uctx))) {
			printk("not sure what to do here. uctx = %p\n", uctx);
			curproc->mm->dump();
			return;
		}

		/* Copy the old user context */
		memcpy(uctx, &tf, sizeof(tf));

		tf.sp = sp;
		tf.a0 = sig;
		tf.a1 = 0;
		tf.a2 = sp; // third argument to a sa_sigaction is the ucontext
		tf.sepc = (rv::xsize_t)handler;
		tf.ra = curproc->sig.ret;
  }

  if (cpu::in_thread()) thd->trap_frame = old_trapframe;

  rv::get_hstate().kernel_sp = previous_kernel_stack;
}

extern "C" void machine_trap(struct rv::regs &tf) {
}
