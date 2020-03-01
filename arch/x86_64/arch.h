
struct x86_64regs {
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

  unsigned long trapno;  // 15
  unsigned long err;     // 16

  unsigned long rip;     // 17
  unsigned long cs;      // 18
  unsigned long rflags;  // 19
  unsigned long rsp;     // 20
  unsigned long ds;      // 21
};
