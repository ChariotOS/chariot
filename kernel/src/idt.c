#include <asm.h>
#include <idt.h>
#include <mem.h>
#include <paging.h>
#include <printk.h>
#include <types.h>

// struct gatedesc idt[NUM_IDT_ENTRIES];

#define EXCP_NAME 0
#define EXCP_MNEMONIC 1
const char *excp_codes[NUM_EXCEPTIONS][2] = {
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

// Set up a normal interrupt/trap gate descriptor.
// - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate.
//   interrupt gate clears FL_IF, trap gate leaves FL_IF alone
// - sel: Code segment selector for interrupt/trap handler
// - off: Offset in code segment for interrupt/trap handler
// - dpl: Descriptor Privilege Level -
//        the privilege level required for software to invoke
//        this interrupt/trap gate explicitly using an int instruction.
#define SETGATE(gate, istrap, sel, off, d)        \
  {                                               \
    (gate).off_15_0 = (u32)(off)&0xffff;          \
    (gate).cs = (sel);                            \
    (gate).args = 0;                              \
    (gate).rsv1 = 0;                              \
    (gate).type = (istrap) ? STS_TG32 : STS_IG32; \
    (gate).s = 0;                                 \
    (gate).dpl = (d);                             \
    (gate).p = 1;                                 \
    (gate).off_31_16 = (u32)(off) >> 16;          \
  }

extern u32 idt_block;

static u32 *idt;

extern void *vectors[];  // in vectors.S: array of 256 entry pointers

static void mkgate(u32 *idt, u32 n, void *kva, u32 pl, u32 trap) {
  u64 addr = (u64)kva;
  n *= 4;
  trap = trap ? 0x8F00 : 0x8E00;  // TRAP vs INTERRUPT gate;
  idt[n + 0] = (addr & 0xFFFF) | ((SEG_KCODE << 3) << 16);
  idt[n + 1] = (addr & 0xFFFF0000) | trap | ((pl & 3) << 13);  // P=1 DPL=pl
  idt[n + 2] = addr >> 32;
  idt[n + 3] = 0;
}

void init_idt(void) {
  u32 *idt = &idt_block;

  // and fill it up with the correct vectors
  for (int n = 0; n < 256; n++) mkgate(idt, n, vectors[n], 0, 0);
  //printk("idt=%p\n", idt);
  lidt(idt, 4096);
}

struct trapframe {
  u64 rax;  // rax
  u64 rbx;
  u64 rcx;
  u64 rdx;
  u64 rbp;
  u64 rsi;
  u64 rdi;
  u64 r8;
  u64 r9;
  u64 r10;
  u64 r11;
  u64 r12;
  u64 r13;
  u64 r14;
  u64 r15;

  u64 trapno;
  u64 err;

  u64 eip;  // rip
  u64 cs;
  u64 eflags;  // rflags
  u64 esp;     // rsp
  u64 ds;      // ss
};



// Processor-defined:
#define TRAP_DIVIDE         0      // divide error
#define TRAP_DEBUG          1      // debug exception
#define TRAP_NMI            2      // non-maskable interrupt
#define TRAP_BRKPT          3      // breakpoint
#define TRAP_OFLOW          4      // overflow
#define TRAP_BOUND          5      // bounds check
#define TRAP_ILLOP          6      // illegal opcode
#define TRAP_DEVICE         7      // device not available
#define TRAP_DBLFLT         8      // double fault
// #define TRAP_COPROC      9      // reserved (not used since 486)
#define TRAP_TSS           10      // invalid task switch segment
#define TRAP_SEGNP         11      // segment not present
#define TRAP_STACK         12      // stack exception
#define TRAP_GPFLT         13      // general protection fault
#define TRAP_PGFLT         14      // page fault
// #define TRAP_RES        15      // reserved
#define TRAP_FPERR         16      // floating point error
#define TRAP_ALIGN         17      // aligment check
#define TRAP_MCHK          18      // machine check
#define TRAP_SIMDERR       19      // SIMD floating point error
// where the trap handler throws us. It is up to this function to sort out
// which trap handler to hand off to
void trap(struct trapframe *tf) {

  if (tf->trapno == TRAP_ILLOP) {
    printk("ILLEGAL INSTRUCION %016x\n", tf->err);
    halt();
  }

  // PAGE FAULT
  if (tf->trapno == TRAP_PGFLT) {
    void *addr = (void *)read_cr2();
    printk("PAGE FAULT %p\n", addr);
    map_page(addr, addr);
    return;
  }


  printk("\n");
  printk(" +++ %s +++ \n", excp_codes[tf->trapno][EXCP_NAME]);
  printk(" +++ UNHANDLED TRAP +++\n");
  printk("\n");
  lidt(0, 0);  // die
  while (1) halt();
}
