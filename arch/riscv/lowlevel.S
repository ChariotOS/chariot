# low.S - All low level functionality (boot and util)
# riscv64 bootloader for ChariotOS
# Nick Wanninger
# 29 December, 2020

#ifdef CONFIG_64BIT
#define RSIZE 8
#define SREG sd
#define LREG ld
#else

#define RSIZE 4
#define SREG sw
#define LREG lw
#endif



#define ROFF(N, R) N*RSIZE (R)

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


.section .text
.globl timervec
.align 4
timervec:
	csrrw a0, mscratch, a0
	SREG a1, 0(a0)
	SREG a2, 8(a0)
	SREG a3, 16(a0)

	# schedule the next timer interrupt
	# by adding interval to mtimecmp.
	LREG a1, 24(a0) # CLINT_MTIMECMP(hart)
	LREG a2, 32(a0) # interval
	LREG a3, 0(a1)
	add a3, a3, a2
	SREG a3, 0(a1)

	# raise a supervisor software interrupt.
	li a1, 2
	csrw sip, a1

	LREG a3, ROFF(2, a0)
	LREG a2, ROFF(1, a0)
	LREG a1, ROFF(0, a0)
	csrrw a0, mscratch, a0

	mret


	#
        # interrupts and exceptions while in supervisor
        # mode come here.
        #
        # push all registers, call kerneltrap(), restore, return.
        #
.globl kernel_trap
.globl kernelvec
.align 4
kernelvec:
	# make room to save registers.
	addi sp, sp, -32 * RSIZE

	# save the registers.
	SREG ra, ROFF(0, sp)
	SREG sp, ROFF(1, sp)
	SREG gp, ROFF(2, sp)
	SREG tp, ROFF(3, sp)
	SREG t0, ROFF(4, sp)
	SREG t1, ROFF(5, sp)
	SREG t2, ROFF(6, sp)
	SREG s0, ROFF(7, sp)
	SREG s1, ROFF(8, sp)
	SREG a0, ROFF(9, sp)
	SREG a1, ROFF(10,sp)
	SREG a2, ROFF(11, sp)
	SREG a3, ROFF(12, sp)
	SREG a4, ROFF(13, sp)
	SREG a5, ROFF(14, sp)
	SREG a6, ROFF(15, sp)
	SREG a7, ROFF(16, sp)
	SREG s2, ROFF(17, sp)
	SREG s3, ROFF(18, sp)
	SREG s4, ROFF(19, sp)
	SREG s5, ROFF(20, sp)
	SREG s6, ROFF(21, sp)
	SREG s7, ROFF(22, sp)
	SREG s8, ROFF(23, sp)
	SREG s9, ROFF(24, sp)
	SREG s10, ROFF(25, sp)
	SREG s11, ROFF(26, sp)
	SREG t3, ROFF(27, sp)
	SREG t4, ROFF(28, sp)
	SREG t5, ROFF(29, sp)
	SREG t6, ROFF(30, sp)

	csrr a0, sepc
	SREG a0, ROFF(31, sp)


# call the C trap handler in trap.c
	mv a0, sp
	call kernel_trap

	# read sepc from the stack now, as we need a0 to be restored
	LREG a0, ROFF(31, sp)
	csrw sepc, a0

	# restore registers.
	LREG ra, ROFF(0, sp)
	LREG sp, ROFF(1, sp)
	LREG gp, ROFF(2, sp)
	# SREG tp, ROFF(3, sp) ;; not this!
	LREG t0, ROFF(4, sp)
	LREG t1, ROFF(5, sp)
	LREG t2, ROFF(6, sp)
	LREG s0, ROFF(7, sp)
	LREG s1, ROFF(8, sp)
	LREG a0, ROFF(9, sp)
	LREG a1, ROFF(10,sp)
	LREG a2, ROFF(11, sp)
	LREG a3, ROFF(12, sp)
	LREG a4, ROFF(13, sp)
	LREG a5, ROFF(14, sp)
	LREG a6, ROFF(15, sp)
	LREG a7, ROFF(16, sp)
	LREG s2, ROFF(17, sp)
	LREG s3, ROFF(18, sp)
	LREG s4, ROFF(19, sp)
	LREG s5, ROFF(20, sp)
	LREG s6, ROFF(21, sp)
	LREG s7, ROFF(22, sp)
	LREG s8, ROFF(23, sp)
	LREG s9, ROFF(24, sp)
	LREG s10, ROFF(25, sp)
	LREG s11, ROFF(26, sp)
	LREG t3, ROFF(27, sp)
	LREG t4, ROFF(28, sp)
	LREG t5, ROFF(29, sp)
	LREG t6, ROFF(30, sp)

	addi sp, sp, 32*RSIZE

	# return to whatever we were doing in the kernel.
	sret



# TODO: 32 vs 64bit :)
.globl context_switch # (addr_of_old_state_ptr, new_state_ptr)
context_switch:
#define CTX_SIZE (13 * RSIZE)

	# a0: address to place stack pointer
	# a1: new stack pointer

	addi sp, sp, -CTX_SIZE # 128 is a round up, as we only need 104. TODO: 

	SREG ra,  ROFF(0,  sp)
	SREG s0,  ROFF(1,  sp)
	SREG s1,  ROFF(2,  sp)
	SREG s2,  ROFF(3,  sp)
	SREG s3,  ROFF(4,  sp)
	SREG s4,  ROFF(5,  sp)
	SREG s5,  ROFF(6,  sp)
	SREG s6,  ROFF(7,  sp)
	SREG s7,  ROFF(8,  sp)
	SREG s8,  ROFF(9,  sp)
	SREG s9,  ROFF(10, sp)
	SREG s10, ROFF(11, sp)
	SREG s11, ROFF(12, sp)

	SREG sp, (a0)
	mv sp, a1


	LREG ra,  ROFF(0,  sp)
	LREG s0,  ROFF(1,  sp)
	LREG s1,  ROFF(2,  sp)
	LREG s2,  ROFF(3,  sp)
	LREG s3,  ROFF(4,  sp)
	LREG s4,  ROFF(5,  sp)
	LREG s5,  ROFF(6,  sp)
	LREG s6,  ROFF(7,  sp)
	LREG s7,  ROFF(8,  sp)
	LREG s8,  ROFF(9,  sp)
	LREG s9,  ROFF(10, sp)
	LREG s10, ROFF(11, sp)
	LREG s11, ROFF(12, sp)

	addi sp, sp, CTX_SIZE
	
	ret
