
/* Mark end of function.  */
#undef END
#define END(function) .size function, .- function


#ifdef CONFIG_RISCV

#ifndef __riscv_xlen
#define __riscv_xlen 64
#endif

/* Macros to handle different pointer/register sizes for 32/64-bit code.  */
#if __riscv_xlen == 64
#define PTRLOG 3
#define SZREG 8
#define REG_S sd
#define REG_L ld
#define REG_SC sc.d
#elif __riscv_xlen == 32
#define PTRLOG 2
#define SZREG 4
#define REG_S sw
#define REG_L lw
#define REG_SC sc.w
#else
#error __riscv_xlen must equal 32 or 64
#endif


#define FREG_L fld
#define FREG_S fsd
#define SZFREG 8

#define ROFF(N, R) N* SZREG(R)
#endif

#ifndef C_LABEL
/* Define a macro we can use to construct the asm name for a C symbol.  */
#define C_LABEL(name) name##:
#endif

#ifndef ENTRY
#define ENTRY(name) \
  .text;            \
  .balign 16;       \
  .global name;     \
  C_LABEL(name)

#endif
