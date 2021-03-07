
/* Mark end of function.  */
#undef END
#define END(function) .size function, .- function


#ifdef CONFIG_RISCV
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


#if !defined __riscv_float_abi_soft
/* For ABI uniformity, reserve 8 bytes for floats, even if double-precision
   floating-point is not supported in hardware.  */
#if defined __riscv_float_abi_double
#define FREG_L fld
#define FREG_S fsd
#define SZFREG 8
#else
#error unsupported FLEN
#endif
#endif

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
