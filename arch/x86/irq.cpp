#include <arch.h>
#include <cpu.h>
#include <paging.h>
#include <pit.h>
#include <printk.h>
#include <sched.h>
#include <syscall.h>
#include <time.h>
#include <util.h>
#include <x86/smp.h>
#include <debug.h>

// implementation of the x86 interrupt request handling system
extern u32 idt_block[];
extern void *vectors[];  // in vectors.S: array of 256 entry pointers

// Processor-defined:
#define TRAP_DIVIDE 0  // divide error
#define TRAP_DEBUG 1   // debug exception
#define TRAP_NMI 2     // non-maskable interrupt
#define TRAP_BRKPT 3   // breakpoint
#define TRAP_OFLOW 4   // overflow
#define TRAP_BOUND 5   // bounds check
#define TRAP_ILLOP 6   // illegal opcode
#define TRAP_DEVICE 7  // device not available
#define TRAP_DBLFLT 8  // double fault
#define TRAP_TSS 10    // invalid task switch segment
#define TRAP_SEGNP 11  // segment not present
#define TRAP_STACK 12  // stack exception
#define TRAP_GPFLT 13  // general protection fault
#define TRAP_PGFLT 14  // page fault
// #define TRAP_RES        15      // reserved
#define TRAP_FPERR 16    // floating point error
#define TRAP_ALIGN 17    // aligment check
#define TRAP_MCHK 18     // machine check
#define TRAP_SIMDERR 19  // SIMD floating point error

#define EXCP_NAME 0
#define EXCP_MNEMONIC 1
const char *excp_codes[128][2] = {
    {"Divide By Zero", "#DE"},
    {"Debug", "#DB"},
    {"Non-maskable Interrupt", "N/A"},
    {"Breakpoint Exception", "#BP"},
    {"Overflow Exception", "#OF"},
    {"Bound Range Exceeded", "#BR"},
    {"Invalid Opcode", "#UD"},
    {"Device Not Available", "#NM"},
    {"Double Fault", "#DF"},
    {"Coproc Segment Overrun", "N/A"},
    {"Invalid TSS", "#TS"},
    {"Segment Not Present", "#NP"},
    {"Stack Segment Fault", "#SS"},
    {"General Protection Fault", "#GP"},
    {"Page Fault", "#PF"},
    {"Reserved", "N/A"},
    {"x86 FP Exception", "#MF"},
    {"Alignment Check", "#AC"},
    {"Machine Check", "#MC"},
    {"SIMD FP Exception", "#XM"},
    {"Virtualization Exception", "#VE"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Reserved", "N/A"},
    {"Security Exception", "#SX"},
    {"Reserved", "N/A"},
};


/* eflags masks */
#define CC_C 0x0001
#define CC_P 0x0004
#define CC_A 0x0010
#define CC_Z 0x0040
#define CC_S 0x0080
#define CC_O 0x0800

#define TF_SHIFT 8
#define IOPL_SHIFT 12
#define VM_SHIFT 17
#define TF_MASK 0x00000100
#define IF_MASK 0x00000200
#define DF_MASK 0x00000400
#define IOPL_MASK 0x00003000
#define NT_MASK 0x00004000
#define RF_MASK 0x00010000
#define VM_MASK 0x00020000
#define AC_MASK 0x00040000
#define VIF_MASK 0x00080000
#define VIP_MASK 0x00100000
#define ID_MASK 0x00200000


#define PGFLT_PRESENT (1 << 0)
#define PGFLT_WRITE (1 << 1)
#define PGFLT_USER (1 << 2)
#define PGFLT_RESERVED (1 << 3)
#define PGFLT_INSTR (1 << 4)



int arch_generate_backtrace(off_t virt_ebp, off_t *buf, size_t bufsz) {
  if (!cpu::in_thread()) return 0;

  mm::space *mm = curthd->proc.mm;
  auto &pt = *mm->pt;

  off_t *phys_ebp;
  off_t virt_return_storage;
  off_t *phys_return_storage;
  off_t ip;

  int i = 0;

  auto translate = [&](off_t virt) -> off_t * {
    if (virt > CONFIG_KERNEL_VIRTUAL_BASE) {
      return (off_t *)virt;
    }

    return (off_t *)pt.translate(virt);
  };


  for (; i < bufsz; i++) {
    // uh oh, you are doing something nefarious
    if (virt_ebp > CONFIG_KERNEL_VIRTUAL_BASE) {
      // for (int j = 0; j < i; j++)
      //   buf[j] = 0;
      // return 0;
    }
    phys_ebp = translate(virt_ebp);
    virt_return_storage = (virt_ebp + sizeof(off_t));
    phys_return_storage = phys_ebp + 1;
    if ((virt_return_storage & ~0xFFF) != (virt_ebp & ~0xFFF)) {
      phys_return_storage = translate(virt_return_storage);
    }

    if (phys_ebp == NULL) break;
    if (phys_return_storage == NULL) break;

    // if the instruction pointer is not mapped, we are done.
    ip = *(off_t *)p2v(phys_return_storage);
    if (translate(ip) == NULL) break;

    buf[i] = ip;

    virt_ebp = *(off_t *)p2v(phys_ebp);
  }
  return i;
}



void dump_backtrace(off_t virt_ebp) {
  int i = 0;
  for (auto ip : debug::generate_backtrace(virt_ebp)) {
    printk("%d - %p\n", i++, ip);
  }
  // printk("Backtrace (ebp=%p):\n", ebp);
}


void arch_dump_backtrace(void) {
  off_t rbp = 0;
  asm volatile("mov %%rbp, %0\n\t" : "=r"(rbp));

  ::dump_backtrace(rbp);
}


/* This exists so we don't have annoying interleaved trapframe dumping */
// static spinlock trapframe_dump_lock;
void dump_trapframe(reg_t *r) {
  // scoped_lock l(trapframe_dump_lock);
  auto *tf = (struct x86_64regs *)r;

  unsigned int eflags = tf->rflags;

#define GET(name) (tf->name)
#define dump(reg) debug::print_register(#reg, GET(reg))


  dump(rax);
  dump(rbx);
  dump(rcx);
  dump(rdx);
  printk_nolock("\n");
  dump(rsi);
  dump(rdi);
  dump(rbp);
  dump(rsp);
  printk_nolock("\n");
  dump(r8);
  dump(r9);
  dump(r10);
  dump(r11);
  printk_nolock("\n");
  dump(r12);
  dump(r13);
  dump(r14);
  dump(r15);
  printk_nolock("\n");
  dump(rip);
  debug::print_register("flg", eflags);
  printk_nolock(" [%c%c%c%c%c%c%c]\n", eflags & DF_MASK ? 'D' : '-', eflags & CC_O ? 'O' : '-', eflags & CC_S ? 'S' : '-',
      eflags & CC_Z ? 'Z' : '-', eflags & CC_A ? 'A' : '-', eflags & CC_P ? 'P' : '-', eflags & CC_C ? 'C' : '-');
  printk_nolock("Backtrace:\n");

#undef dump
  auto bt = debug::generate_backtrace(tf->rbp);

  int i = 0;
  for (auto ip : bt) {
    printk_nolock("% 2d - %p\n", i++, ip);
  }
  printk_nolock("Pass this into addr2line for %s:\n", curproc->name.get());
  for (auto ip : bt) {
    printk_nolock("0x%p ", ip);
  }
  printk_nolock("\n");
}



void arch::irq::eoi(int i) {
  if (i >= 32) {
    int pic_irq = i;
    if (pic_irq >= 8) {
      outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
  }
  smp::lapic_eoi();
}

void arch::irq::enable(int num) {
#ifdef CONFIG_SMP
  smp::ioapicenable(num, /* TODO */ 0);
#endif
  pic_enable(num);
}

void arch::irq::disable(int num) {
  // smp::ioapicdisable(num);
  pic_disable(num);
}

static void mkgate(u32 *idt, u32 n, void *kva, u32 pl, u32 trap) {
  u64 addr = (u64)kva;
  n *= 4;
  trap = trap ? 0x8F00 : 0x8E00;  // TRAP vs INTERRUPT gate;
  idt[n + 0] = (addr & 0xFFFF) | ((SEG_KCODE << 3) << 16);
  idt[n + 1] = (addr & 0xFFFF0000) | trap | ((pl & 0b11) << 13);  // P=1 DPL=pl
  idt[n + 2] = addr >> 32;
  idt[n + 3] = 0;
}


// boot time tick handler
// Will be replaced by the lapic
static void tick_handle(int i, reg_t *regs, void *) {
  auto &cpu = cpu::current();

  u64 now = arch_read_timestamp();
  cpu.kstat.tsc_per_tick = now - cpu.kstat.last_tick_tsc;
  cpu.kstat.last_tick_tsc = now;
  cpu.kstat.ticks++;

  irq::eoi(32 /* IRQ_TICK */);
  sched::handle_tick(cpu.kstat.ticks);
  return;
}

static void unknown_exception(int i, reg_t *regs) {
  auto *tf = (struct x86_64regs *)regs;

  KERR("KERNEL PANIC\n");
  KERR("CPU EXCEPTION: %s\n", excp_codes[tf->trapno][EXCP_NAME]);
  KERR("the system was running for %d ticks\n");

  KERR("\n");
  KERR("Stats for nerds:\n");
  KERR("INT=%016zx  ERR=%016zx\n", tf->trapno, tf->err);
  KERR("ESP=%016zx  EIP=%016zx\n", tf->rsp, tf->rip);
  KERR("CR2=%016zx  CR3=%016zx\n", read_cr2(), read_cr3());
  KERR("\n");
  KERR("SYSTEM HALTED. File a bug report please:\n");
  KERR("  repo: github.com/nickwanninger/chariot\n");

  KERR("\n");

  lidt(0, 0);  // die
  while (1) {
  };
}

static void dbl_flt_handler(int i, reg_t *tf) {
  printk("DOUBLE FAULT!\n");
  printk("&i=%p\n", &i);

  unknown_exception(i, tf);
  while (1) {
  }
}

extern const char *ksym_find(off_t);




void handle_fatal(const char *name, int fatal_signal, reg_t *regs) {
  auto *tf = (struct x86_64regs *)regs;


  arch_disable_ints();
  printk_nolock("FATAL SIGNAL %d\n", fatal_signal);
  printk_nolock("==================================================================\n");
  // TODO:
  printk_nolock("%s in pid %d, tid %d @ %p\n", name, curthd->pid, curthd->tid, tf->rip);

  debug::print_register("Bad address", read_cr2());
  // printk_nolock("              info = ");
  // if (tf->err & PGFLT_PRESENT) printk_nolock("PRESENT ");
  // if (tf->err & PGFLT_WRITE) printk_nolock("WRITE ");
  // if (tf->err & PGFLT_USER) printk_nolock("USER ");
  // if (tf->err & PGFLT_INSTR) printk_nolock("INSTR ");
  // printk_nolock("\n");

  printk_nolock("\n");

  // arch_disable_ints();
  dump_trapframe(regs);
  // arch_enable_ints();
  KERR("==================================================================\n");

  arch_enable_ints();
  curproc->terminate(fatal_signal);
}

static void gpf_handler(int i, reg_t *regs) { handle_fatal("General Protection Fault", SIGSEGV, regs); }

static void illegal_instruction_handler(int i, reg_t *regs) { handle_fatal("Illegal Instruction", SIGILL, regs); }

extern "C" void syscall_handle(int i, reg_t *tf, void *);



static void pgfault_handle(int i, reg_t *regs) {
  auto *tf = (struct x86_64regs *)regs;
  uint64_t addr = read_cr2();
  void *page = (void *)(addr & ~0xFFF);

  auto proc = curproc;
  if (curproc == NULL) {
    panic("not in a proc while pagefaulting (rip=%p, addr=%p)\n", tf->rip, read_cr2());
    // arch_dump_backtrace();
    // lookup the kernel proc if we aren't in one!
    proc = sched::proc::kproc();
  };

  if (proc) {
    int err = 0;

    // TODO: read errors
    if (tf->err & PGFLT_USER) err |= FAULT_PERM;
    if (tf->err & PGFLT_WRITE) err |= FAULT_WRITE;
    if (tf->err & PGFLT_INSTR) err |= FAULT_EXEC;

    if ((tf->err & PGFLT_USER) == 0) {
      // TODO
    }

    int res = proc->mm->pagefault((off_t)page, err);
    if (res == -1) handle_fatal("Segmentation Violation", SIGSEGV, regs);
  } else {
    panic("page fault in kernel code (no proc)\n");
  }
}

extern void pit_irq_handler(int i, reg_t *);


void (*isr_functions[32])(int, reg_t *);


int arch::irq::init(void) {
  init_pit();
  u32 *idt = (u32 *)&idt_block;

  // fill up the idt with the correct trap vectors.
  for (int n = 0; n < 130; n++)
    mkgate(idt, n, vectors[n], 0, 0);

  int i;
  // for (i = 32; i < 48; i++) irq::disable(i);

  // handle all the <32 irqs as unknown
  for (i = 0; i < 32; i++) {
    isr_functions[i] = unknown_exception;
  }
  isr_functions[TRAP_DBLFLT] = dbl_flt_handler;
  isr_functions[TRAP_PGFLT] = pgfault_handle;
  isr_functions[TRAP_GPFLT] = gpf_handler;
  isr_functions[TRAP_ILLOP] = illegal_instruction_handler;

  for (i = 32; i < 48; i++) {
    // ::irq::install(i, unknown_hardware, "Unknown Hardware");
  }



  pic_disable(34);

  init_pit();

  ::irq::install(0, tick_handle, "Preemption Tick");


  // setup the ancient systemcall interface
  mkgate(idt, 0x80, vectors[0x80], 3, 0);
  // ::irq::install(0x80 - 32, syscall_handle, "System Call");

  KINFO("Registered basic interrupt handlers\n");

  // and load the idt into the processor. It is a page in memory
  lidt(idt, 4096);

  return 0;
}


extern "C" void x86_enter_userspace(x86_64regs *);

// just forward the trap on to the irq subsystem
// This function is called from arch/x86/trap.asm
extern "C" void trap(reg_t *regs) {
  auto *tf = (struct x86_64regs *)regs;
  bool from_userspace = tf->cs == 0x23;


  {
    bool ts = time::stabilized();
    if (cpu::in_thread() && from_userspace) {
      auto thd = curthd;
      thd->utime_us += time::now_us() - thd->last_start_utime_us;
      thd->trap_frame = regs;
    }

    int nr = tf->trapno;

    off_t bt[64];
    int count = 0;

    // TODO profiler??? :)
    if (false && curproc && from_userspace) count = arch_generate_backtrace(tf->rbp, bt, 64);


    if (nr == 0x80) {
      arch_enable_ints();
      syscall_handle(0x80, regs, NULL);
    } else {
      if (nr >= 32) {
        irq::dispatch(nr - 32, regs);
      } else {
        arch_enable_ints();
        isr_functions[nr](nr, regs);
      }
    }
  }



  irq::eoi(tf->trapno);


  // TODO: generalize
  sched::before_iret(from_userspace);

  if (from_userspace) {
    /* Handle signals */
    int sig = 0;
    void *handler = NULL;
    if (sched::claim_next_signal(sig, handler) != -1) {
      // arch_disable_ints();
      uint64_t sp = tf->rsp;
      sp -= 128;  // red zone BS
      int frame_size = sizeof(x86_64regs);
      sp -= frame_size;

      auto *uctx = (x86_64regs *)sp;

      /* save the old context to the user stack */
      if (!VALIDATE_RDWR(uctx, frame_size)) {
        printk("not sure what to do here. uctx = %p\n", uctx);
        curproc->mm->dump();
        return;
      }

      /* Copy the old user context */
      memcpy(uctx, tf, frame_size);

      tf->rdi = sig;
      tf->rsi = 0;
      tf->rdx = sp;

      // push the return address
      sp -= 8;
      *(uint64_t *)sp = curproc->sig.ret;
      tf->rsp = sp;
      tf->rip = (uint64_t)handler;

      tf->cs = (SEG_UCODE << 3) | DPL_USER;
      tf->ds = (SEG_UDATA << 3) | DPL_USER;
      x86_enter_userspace(tf);
      // arch_enable_ints();
    }
  }
}
