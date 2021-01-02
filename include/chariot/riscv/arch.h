#pragma once

/* sanity check */
#ifdef CONFIG_RISCV


#include <types.h>

#define read_csr(name)                         \
  ({                                           \
    rv::xsize_t x;                             \
    asm volatile("csrr %0, " #name : "=r"(x)); \
    x;                                         \
  })

#define write_csr(csr, val)                                             \
  ({                                                                    \
    unsigned long __v = (unsigned long)(val);                           \
    __asm__ __volatile__("csrw " #csr ", %0" : : "rK"(__v) : "memory"); \
  })


#define TICK_INTERVAL (CONFIG_RISCV_CLOCKS_PER_SECOND / CONFIG_TICKS_PER_SECOND)

namespace dtb {
  struct fdt_header;
};

namespace rv /* risc-v namespace */ {


  /* xsize-bit integer, as specified in the ISA manual (64 on rv64, 32 on rv32) */
  using xsize_t = uint64_t;

  static inline rv::xsize_t mhartid(void) {
    rv::xsize_t x;
    asm volatile("csrr %0, mhartid" : "=r"(x));
    return x;
  }


  struct scratch {
    rv::xsize_t bak[3];   /* Used in timervec */
    rv::xsize_t tca;      /* address of this HART's MTIMECMP register */
    rv::xsize_t interval; /* Timer interval */

    int hartid;

    struct dtb::fdt_header *dtb; /* Device tree binary */
    /* ... */
  };



  /* Get the current scratch pointer on this hart */
  struct scratch &get_scratch(void);

  static inline auto hartid(void) { return rv::get_scratch().hartid; }


  struct regs {
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

		/* Exception PC */
		rv::xsize_t sepc;
    /* Missing floating point registers in the kernel trap frame */
  };




  // Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11)  // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)  // machine-mode interrupt enable.

  static inline rv::xsize_t get_mstatus(void) {
    rv::xsize_t x;
    asm volatile("csrr %0, mstatus" : "=r"(x));
    return x;
  }

  static inline void set_mstatus(rv::xsize_t x) { asm volatile("csrw mstatus, %0" : : "r"(x)); }


  // machine exception program counter, holds the
  // instruction address to which a return from
  // exception will go.
  static inline void set_mepc(rv::xsize_t x) { asm volatile("csrw mepc, %0" : : "r"(x)); }


  // Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5)  // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4)  // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable


  static inline rv::xsize_t get_sstatus() {
    rv::xsize_t x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
  }

  static inline void set_sstatus(rv::xsize_t x) { asm volatile("csrw sstatus, %0" : : "r"(x)); }

  // Supervisor Interrupt Pending
  static inline rv::xsize_t get_sip() {
    rv::xsize_t x;
    asm volatile("csrr %0, sip" : "=r"(x));
    return x;
  }

  static inline void set_sip(rv::xsize_t x) { asm volatile("csrw sip, %0" : : "r"(x)); }

  // Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9)  // external
#define SIE_STIE (1L << 5)  // timer
#define SIE_SSIE (1L << 1)  // software
  static inline rv::xsize_t get_sie() {
    rv::xsize_t x;
    asm volatile("csrr %0, sie" : "=r"(x));
    return x;
  }

  static inline void set_sie(rv::xsize_t x) { asm volatile("csrw sie, %0" : : "r"(x)); }


  // Machine-mode Interrupt Enable
#define MIE_MEIE (1L << 11)  // external
#define MIE_MTIE (1L << 7)  // timer
#define MIE_MSIE (1L << 3)  // software
  static inline rv::xsize_t get_mie() {
    rv::xsize_t x;
    asm volatile("csrr %0, mie" : "=r"(x));
    return x;
  }

  static inline void set_mie(rv::xsize_t x) { asm volatile("csrw mie, %0" : : "r"(x)); }


  // machine exception program counter, holds the
  // instruction address to which a return from
  // exception will go.
  static inline void set_sepc(rv::xsize_t x) { asm volatile("csrw sepc, %0" : : "r"(x)); }

  static inline rv::xsize_t get_sepc() {
    rv::xsize_t x;
    asm volatile("csrr %0, sepc" : "=r"(x));
    return x;
  }

  // Machine Exception Delegation
  static inline rv::xsize_t get_medeleg() {
    rv::xsize_t x;
    asm volatile("csrr %0, medeleg" : "=r"(x));
    return x;
  }

  static inline void set_medeleg(rv::xsize_t x) { asm volatile("csrw medeleg, %0" : : "r"(x)); }

  // Machine Interrupt Delegation
  static inline rv::xsize_t get_mideleg() {
    rv::xsize_t x;
    asm volatile("csrr %0, mideleg" : "=r"(x));
    return x;
  }

  static inline void set_mideleg(rv::xsize_t x) { asm volatile("csrw mideleg, %0" : : "r"(x)); }

  // Supervisor Trap-Vector Base Address
  // low two bits are mode.
  static inline void set_stvec(rv::xsize_t x) { asm volatile("csrw stvec, %0" : : "r"(x)); }

  static inline rv::xsize_t get_stvec() {
    rv::xsize_t x;
    asm volatile("csrr %0, stvec" : "=r"(x));
    return x;
  }

  // Machine-mode interrupt vector
  static inline void set_mtvec(rv::xsize_t x) { asm volatile("csrw mtvec, %0" : : "r"(x)); }

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((rv::xsize_t)pagetable) >> 12))

  // supervisor address translation and protection;
  // holds the address of the page table.
  static inline void set_satp(rv::xsize_t x) { asm volatile("csrw satp, %0" : : "r"(x)); }

  static inline rv::xsize_t get_satp() {
    rv::xsize_t x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
  }

  // Supervisor Scratch register, for early trap handler in trampoline.S.
  static inline void set_sscratch(rv::xsize_t x) { asm volatile("csrw sscratch, %0" : : "r"(x)); }

  static inline void set_mscratch(rv::xsize_t x) { asm volatile("csrw mscratch, %0" : : "r"(x)); }
  static inline rv::xsize_t get_mscratch(void) {
    rv::xsize_t x;
    asm volatile("csrr %0, mscratch" : "=r"(x));
    return x;
  }

  // Supervisor Trap Cause
  static inline rv::xsize_t get_scause() {
    rv::xsize_t x;
    asm volatile("csrr %0, scause" : "=r"(x));
    return x;
  }

  // Supervisor Trap Value
  static inline rv::xsize_t get_stval() {
    rv::xsize_t x;
    asm volatile("csrr %0, stval" : "=r"(x));
    return x;
  }

  // Machine-mode Counter-Enable
  static inline void set_mcounteren(rv::xsize_t x) { asm volatile("csrw mcounteren, %0" : : "r"(x)); }

  static inline rv::xsize_t get_mcounteren() {
    rv::xsize_t x;
    asm volatile("csrr %0, mcounteren" : "=r"(x));
    return x;
  }

  // machine-mode cycle counter
  static inline rv::xsize_t get_time() {
    rv::xsize_t x;
    asm volatile("csrr %0, time" : "=r"(x));
    return x;
  }

  // enable device interrupts
  static inline void intget_on() { set_sstatus(get_sstatus() | SSTATUS_SIE); }

  // disable device interrupts
  static inline void intget_off() { set_sstatus(get_sstatus() & ~SSTATUS_SIE); }

  // are device interrupts enabled?
  static inline int intget_get() {
    rv::xsize_t x = get_sstatus();
    return (x & SSTATUS_SIE) != 0;
  }

  static inline rv::xsize_t get_sp() {
    rv::xsize_t x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
  }

  // read and write tp, the thread pointer, which holds
  // this core's hartid (core number), the index into cpus[].
  static inline rv::xsize_t get_tp() {
    rv::xsize_t x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
  }

  static inline void set_tp(rv::xsize_t x) { asm volatile("mv tp, %0" : : "r"(x)); }

  static inline rv::xsize_t get_ra() {
    rv::xsize_t x;
    asm volatile("mv %0, ra" : "=r"(x));
    return x;
  }

  // flush the TLB.
  static inline void sfence_vma() {
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
  }



  // are device interrupts enabled?
  static inline int intr_enabled() {
    rv::xsize_t x = read_csr(sstatus);
    return (x & SSTATUS_SIE) != 0;
  }


  // enable device interrupts
  static inline void intr_on() { set_sstatus(get_sstatus() | SSTATUS_SIE); }

  // disable device interrupts
  static inline void intr_off() { set_sstatus(get_sstatus() & ~SSTATUS_SIE); }



}  // namespace rv
#endif
