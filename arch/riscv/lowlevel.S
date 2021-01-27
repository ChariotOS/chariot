# lowlevel.S - All low level functionality (boot and util)
# riscv64 bootloader for ChariotOS
# Nick Wanninger
# 29 December, 2020
#include <asmdefs.h>




.option norvc # Disable instruction compression
.section .data


.section .text.init

.global _start
_start:
	
	# Disable linker instruction relaxation for the `la` instruction below.
	# This disallows the assembler from assuming that `gp` is already initialized.
	# This causes the value stored in `gp` to be calculated from `pc`.
.option push
.option norelax
	# la		gp, _global_pointer
.option pop

	# we calculate the starting stack by _stack_start + (2*4096 * (mhartid + 1))
  # so each HART gets 2 pages of stack
	la    sp, _stack_start
	li    t0, CONFIG_RISCV_BOOTSTACK_SIZE * 4096 # 2 page stack
	csrr  t1, mhartid
  addi  t1, t1, 1 # t1 = mhartid + 1
	mul   t0, t0, t1 # t1 = SIZE * (mhartid + 1)
	add   sp, sp, t0 # sp = stack_start + (SIZE * (mhartid + 1))

	# call into kstart in machine mode.
	call kstart


.global mret_stackchange
mret_stackchange:
	add sp, a0, zero
	mret
	

.global timervec
timervec:
	csrrw a0, mscratch, a0
	REG_S a1, ROFF(0, a0)
	REG_S a2, ROFF(1, a0)
	REG_S a3, ROFF(2, a0)

	# schedule the next timer interrupt
	# by adding interval to mtimecmp.
	REG_L a1, 24(a0) # CLINT_MTIMECMP(hart)
	REG_L a2, 32(a0) # interval
	REG_L a3, 0(a1)
	add a3, a3, a2
	REG_S a3, 0(a1)

	# raise a supervisor software interrupt.
	li a1, 2
	csrw sip, a1

	REG_L a3, ROFF(2, a0)
	REG_L a2, ROFF(1, a0)
	REG_L a1, ROFF(0, a0)
	csrrw a0, mscratch, a0

	mret



	
# interrupts and exceptions while in supervisor
# mode come here.
#
# push all registers, call kerneltrap(), restore, return.
#

ENTRY(kernelvec)

	# make room to save registers.
	addi sp, sp, -32 * SZREG

	# save the registers.
	REG_S ra, ROFF(0, sp)
	REG_S sp, ROFF(1, sp)
	REG_S gp, ROFF(2, sp)
	REG_S tp, ROFF(3, sp)
	REG_S t0, ROFF(4, sp)
	REG_S t1, ROFF(5, sp)
	REG_S t2, ROFF(6, sp)
	REG_S s0, ROFF(7, sp)
	REG_S s1, ROFF(8, sp)
	REG_S a0, ROFF(9, sp)
	REG_S a1, ROFF(10,sp)
	REG_S a2, ROFF(11, sp)
	REG_S a3, ROFF(12, sp)
	REG_S a4, ROFF(13, sp)
	REG_S a5, ROFF(14, sp)
	REG_S a6, ROFF(15, sp)
	REG_S a7, ROFF(16, sp)
	REG_S s2, ROFF(17, sp)
	REG_S s3, ROFF(18, sp)
	REG_S s4, ROFF(19, sp)
	REG_S s5, ROFF(20, sp)
	REG_S s6, ROFF(21, sp)
	REG_S s7, ROFF(22, sp)
	REG_S s8, ROFF(23, sp)
	REG_S s9, ROFF(24, sp)
	REG_S s10, ROFF(25, sp)
	REG_S s11, ROFF(26, sp)
	REG_S t3, ROFF(27, sp)
	REG_S t4, ROFF(28, sp)
	REG_S t5, ROFF(29, sp)
	REG_S t6, ROFF(30, sp)

	csrr a0, sepc
	REG_S a0, ROFF(31, sp)


# call the C trap handler in trap.c
	mv a0, sp
	call kernel_trap

	# read sepc from the stack now, as we need a0 to be restored
	REG_L a0, ROFF(31, sp)
	csrw sepc, a0

	# restore registers.
	REG_L ra, ROFF(0, sp)
	REG_L sp, ROFF(1, sp)
	REG_L gp, ROFF(2, sp)
	# REG_S tp, ROFF(3, sp) ;; not this!
	REG_L t0, ROFF(4, sp)
	REG_L t1, ROFF(5, sp)
	REG_L t2, ROFF(6, sp)
	REG_L s0, ROFF(7, sp)
	REG_L s1, ROFF(8, sp)
	REG_L a0, ROFF(9, sp)
	REG_L a1, ROFF(10,sp)
	REG_L a2, ROFF(11, sp)
	REG_L a3, ROFF(12, sp)
	REG_L a4, ROFF(13, sp)
	REG_L a5, ROFF(14, sp)
	REG_L a6, ROFF(15, sp)
	REG_L a7, ROFF(16, sp)
	REG_L s2, ROFF(17, sp)
	REG_L s3, ROFF(18, sp)
	REG_L s4, ROFF(19, sp)
	REG_L s5, ROFF(20, sp)
	REG_L s6, ROFF(21, sp)
	REG_L s7, ROFF(22, sp)
	REG_L s8, ROFF(23, sp)
	REG_L s9, ROFF(24, sp)
	REG_L s10, ROFF(25, sp)
	REG_L s11, ROFF(26, sp)
	REG_L t3, ROFF(27, sp)
	REG_L t4, ROFF(28, sp)
	REG_L t5, ROFF(29, sp)
	REG_L t6, ROFF(30, sp)

	addi sp, sp, 32*SZREG

	# return to whatever we were doing in the kernel.
	sret

END(kernelvec)

# TODO: 32 vs 64bit :)
ENTRY(context_switch)
#define CTX_SIZE (13 * SZREG)

	# a0: address to place stack pointer
	# a1: new stack pointer

	addi sp, sp, -CTX_SIZE # 128 is a round up, as we only need 104. TODO: 

	REG_S ra,  ROFF(0,  sp)
	REG_S s0,  ROFF(1,  sp)
	REG_S s1,  ROFF(2,  sp)
	REG_S s2,  ROFF(3,  sp)
	REG_S s3,  ROFF(4,  sp)
	REG_S s4,  ROFF(5,  sp)
	REG_S s5,  ROFF(6,  sp)
	REG_S s6,  ROFF(7,  sp)
	REG_S s7,  ROFF(8,  sp)
	REG_S s8,  ROFF(9,  sp)
	REG_S s9,  ROFF(10, sp)
	REG_S s10, ROFF(11, sp)
	REG_S s11, ROFF(12, sp)

	REG_S sp, (a0)
	mv sp, a1


	REG_L ra,  ROFF(0,  sp)
	REG_L s0,  ROFF(1,  sp)
	REG_L s1,  ROFF(2,  sp)
	REG_L s2,  ROFF(3,  sp)
	REG_L s3,  ROFF(4,  sp)
	REG_L s4,  ROFF(5,  sp)
	REG_L s5,  ROFF(6,  sp)
	REG_L s6,  ROFF(7,  sp)
	REG_L s7,  ROFF(8,  sp)
	REG_L s8,  ROFF(9,  sp)
	REG_L s9,  ROFF(10, sp)
	REG_L s10, ROFF(11, sp)
	REG_L s11, ROFF(12, sp)

	addi sp, sp, CTX_SIZE

	ret

END(context_switch)
