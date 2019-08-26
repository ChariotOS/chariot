#ifndef __IDT__
#define __IDT__

#include <asm.h>
#include <types.h>

#define NUM_IDT_ENTRIES 256
#define NUM_EXCEPTIONS 32

#define T_DIVIDE 0  // divide error
#define T_DEBUG 1   // debug exception
#define T_NMI 2     // non-maskable interrupt
#define T_BRKPT 3   // breakpoint
#define T_OFLOW 4   // overflow
#define T_BOUND 5   // bounds check
#define T_ILLOP 6   // illegal opcode
#define T_DEVICE 7  // device not available
#define T_DBLFLT 8  // double fault
// #define T_COPROC      9      // reserved (not used since 486)
#define T_TSS 10    // invalid task switch segment
#define T_SEGNP 11  // segment not present
#define T_STACK 12  // stack exception
#define T_GPFLT 13  // general protection fault
#define T_PGFLT 14  // page fault
// #define T_RES        15      // reserved
#define T_FPERR 16    // floating point error
#define T_ALIGN 17    // aligment check
#define T_MCHK 18     // machine check
#define T_SIMDERR 19  // SIMD floating point error

// These are arbitrarily chosen, but with care not to overlap
// processor defined exceptions or interrupt vectors.
#define T_SYSCALL 64   // system call
#define T_DEFAULT 500  // catchall

#define T_IRQ0 32  // IRQ 0 corresponds to int T_IRQ

#define IRQ_TIMER 0
#define IRQ_KBD 1
#define IRQ_COM1 4
#define IRQ_IDE 14
#define IRQ_ERROR 19
#define IRQ_SPURIOUS 31



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


struct gatedesc {
  u32 off_15_0 : 16;   // low 16 bits of offset in segment
  u32 cs : 16;         // code segment selector
  u32 args : 5;        // # args, 0 for interrupt/trap gates
  u32 rsv1 : 3;        // reserved(should be zero I guess)
  u32 type : 4;        // type(STS_{TG,IG32,TG32})
  u32 s : 1;           // must be 0 (system)
  u32 dpl : 2;         // descriptor(meaning new) privilege level
  u32 p : 1;           // Present
  u32 off_31_16 : 16;  // high bits of offset in segment
};

struct idt_desc {
  uint16_t size;
  uint64_t base_addr;
} __packed;

void init_idt(void);


typedef void (*interrupt_handler_t) (int intr, struct trapframe *);

void interrupt_register(int i, interrupt_handler_t handler);
void interrupt_enable(int i);
void interrupt_disable(int i);
void interrupt_block();
void interrupt_unblock();
void interrupt_wait();

u64 get_ticks(void);

#endif
