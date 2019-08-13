#include <asm.h>
#include <idt.h>
#include <mem.h>
#include <paging.h>
#include <pit.h>
#include <printk.h>
#include <types.h>
#include <vga.h>

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
// #define TRAP_COPROC      9      // reserved (not used since 486)
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

u64 ticks = 0;

extern u32 idt_block[];

extern void *vectors[];  // in vectors.S: array of 256 entry pointers

static interrupt_handler_t interrupt_handler_table[48];
static uint32_t interrupt_count[48];
static uint8_t interrupt_spurious[48];

static void mkgate(u32 *idt, u32 n, void *kva, u32 pl, u32 trap) {
  u64 addr = (u64)kva;
  n *= 4;
  trap = trap ? 0x8F00 : 0x8E00;  // TRAP vs INTERRUPT gate;
  idt[n + 0] = (addr & 0xFFFF) | ((SEG_KCODE << 3) << 16);
  idt[n + 1] = (addr & 0xFFFF0000) | trap | ((pl & 3) << 13);  // P=1 DPL=pl
  idt[n + 2] = addr >> 32;
  idt[n + 3] = 0;
}

static void interrupt_acknowledge(int i) {
  if (i < 32) {
    /* do nothing */
  } else {
    pic_ack(i - 32);
  }
}

void interrupt_block() { asm("cli"); }

void interrupt_unblock() { asm("sti"); }

void interrupt_wait() {
  asm("sti");
  asm("hlt");
}

void interrupt_enable(int i) {
  if (i < 32) {
    /* do nothing */
  } else {
    pic_enable(i - 32);
  }
}

void interrupt_disable(int i) {
  if (i < 32) {
    /* do nothing */
  } else {
    pic_disable(i - 32);
  }
}


#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

static void unknown_exception(int i, struct trapframe *tf) {
  auto color = vga::make_color(vga::color::white, vga::color::blue);
  vga::clear_screen(vga::make_entry(' ', color));
  vga::set_color(vga::color::white, vga::color::blue);


#define INDENT "                  "
#define BORDER INDENT "+========================================+\n"


  for (int i = 0; i < 4; i++) printk("\n");

  printk(BORDER);
  printk("\n");
  printk(INDENT "             ! KERNEL PANIC !\n");
  printk("\n");

  printk(INDENT "CPU EXCEPTION: %s\n", excp_codes[tf->trapno][EXCP_NAME]);
  printk(INDENT "the system was running for %d ticks\n", ticks);

  printk("\n");
  printk(INDENT "Stats for nerds:\n");
  printk(INDENT "INT=%016x  ERR=%016x\n", tf->trapno,
         tf->err);
  printk(INDENT "ESP=%016x  EIP=%016x\n", tf->esp, tf->eip);
  printk("\n");
  printk(INDENT "SYSTEM HALTED. File a bug report please:\n");
  printk(INDENT "  repo: github.com/nickwanninger/nOS\n");

  printk("\n");
  printk(INDENT " %s\n", GIT_REVISION);
  printk("\n");
  printk(BORDER);

  lidt(0, 0);  // die
  while (1) {};
}

static void pgfault_handle(int i, struct trapframe *tf) {
  // void *addr = (void *)(read_cr2() & ~0xFFF);
  printk("EIP: %p\n", tf->eip);
  printk("ERR: %p\n", tf->err);
  panic("PGFLT %p\n", read_cr2());
  // paging::map_page(addr, addr);
  return;
}

static void tick_handle(int i, struct trapframe *tf) {
  ticks++;
  return;
}

static void unknown_hardware(int i, struct trapframe *tf) {
  if (!interrupt_spurious[i]) {
    printk("interrupt: spurious interrupt %d\n", i);
  }
  interrupt_spurious[i]++;
}

void interrupt_register(int i, interrupt_handler_t handler) {
  interrupt_handler_table[i] = handler;
}

void init_idt(void) {
  u32 *idt = (u32 *)&idt_block;
  // fill up the idt with the correct trap vectors.
  for (int n = 0; n < 256; n++) mkgate(idt, n, vectors[n], 0, 0);

  int i;
  for (i = 32; i < 48; i++) {
    interrupt_disable(i);
    interrupt_acknowledge(i);
  }
  for (i = 0; i < 32; i++) {
    interrupt_handler_table[i] = unknown_exception;
    interrupt_spurious[i] = 0;
    interrupt_count[i] = 0;
  }
  for (i = 32; i < 48; i++) {
    interrupt_handler_table[i] = unknown_hardware;
    interrupt_spurious[i] = 0;
    interrupt_count[i] = 0;
  }

  interrupt_register(TRAP_PGFLT, pgfault_handle);
  interrupt_register(32, tick_handle);

  interrupt_unblock();
  // and load the idt into the processor. It is a page in memory
  lidt(idt, 4096);
}

// where the trap handler throws us. It is up to this function to sort out
// which trap handler to hand off to
extern "C" void trap(struct trapframe *tf) {
  extern void pic_send_eoi(void);

  int i = tf->trapno;
  (interrupt_handler_table[i])(i, tf);
  interrupt_acknowledge(i);
  interrupt_count[i]++;
}
