#pragma once

#ifdef CONFIG_X86
struct ucontext {
  unsigned long rax;  // 0
  unsigned long rdi;  // 1
  unsigned long rsi;  // 2
  unsigned long rdx;  // 3
  unsigned long r10;  // 4
  unsigned long r8;   // 5
  unsigned long r9;   // 6

  unsigned long rbx;  // 7
  unsigned long rcx;  // 8
  unsigned long rbp;  // 9
  unsigned long r11;  // 10
  unsigned long r12;  // 11
  unsigned long r13;  // 12
  unsigned long r14;  // 13
  unsigned long r15;  // 14

  unsigned long resv1;  // 15
  unsigned long resv2;  // 16

  unsigned long rip;     // 17
  unsigned long resv3;   // 18
  unsigned long rflags;  // 19
  unsigned long rsp;     // 20
  unsigned long resv4;   // 21
};
#endif

#ifdef CONFIG_RISCV


struct ucontext {
  unsigned long ra; /* x1: Return address */
  unsigned long sp; /* x2: Stack pointer */
  unsigned long gp; /* x3: Global pointer */
  unsigned long tp; /* x4: Thread Pointer */
  unsigned long t0; /* x5: Temp 0 */
  unsigned long t1; /* x6: Temp 1 */
  unsigned long t2; /* x7: Temp 2 */
  unsigned long s0; /* x8: Saved register / Frame Pointer */
  unsigned long s1; /* x9: Saved register */
  unsigned long a0; /* Arguments, you get it :) */
  unsigned long a1;
  unsigned long a2;
  unsigned long a3;
  unsigned long a4;
  unsigned long a5;
  unsigned long a6;
  unsigned long a7;
  unsigned long s2; /* More Saved registers... */
  unsigned long s3;
  unsigned long s4;
  unsigned long s5;
  unsigned long s6;
  unsigned long s7;
  unsigned long s8;
  unsigned long s9;
  unsigned long s10;
  unsigned long s11;
  unsigned long t3; /* More temporaries */
  unsigned long t4;
  unsigned long t5;
  unsigned long t6;

  /* Exception PC */
  unsigned long pc;    /* 31 fault register */
  unsigned long resv1; /* 32 */
  unsigned long resv2; /* 33 */
  unsigned long resv3; /* 34 */
  unsigned long resv4; /* 35 */
  /* Missing floating point registers in the kernel trap frame */
};

#endif
