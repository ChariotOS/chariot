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
#if 0

	# make room to save registers.
	addi sp, sp, -256

	# save the registers.
	sd ra, 0(sp)
	sd sp, 8(sp)
	sd gp, 16(sp)
	sd tp, 24(sp)
	sd t0, 32(sp)
	sd t1, 40(sp)
	sd t2, 48(sp)
	sd s0, 56(sp)
	sd s1, 64(sp)
	sd a0, 72(sp)
	sd a1, 80(sp)
	sd a2, 88(sp)
	sd a3, 96(sp)
	sd a4, 104(sp)
	sd a5, 112(sp)
	sd a6, 120(sp)
	sd a7, 128(sp)
	sd s2, 136(sp)
	sd s3, 144(sp)
	sd s4, 152(sp)
	sd s5, 160(sp)
	sd s6, 168(sp)
	sd s7, 176(sp)
	sd s8, 184(sp)
	sd s9, 192(sp)
	sd s10, 200(sp)
	sd s11, 208(sp)
	sd t3, 216(sp)
	sd t4, 224(sp)
	sd t5, 232(sp)
	sd t6, 240(sp)

# call the C trap handler in trap.c
	mv a0, sp
	call kerneltrap

	# restore registers.
	ld ra, 0(sp)
	ld sp, 8(sp)
	ld gp, 16(sp)
	# not this, in case we moved CPUs: ld tp, 24(sp)
	ld t0, 32(sp)
	ld t1, 40(sp)
	ld t2, 48(sp)
	ld s0, 56(sp)
	ld s1, 64(sp)
	ld a0, 72(sp)
	ld a1, 80(sp)
	ld a2, 88(sp)
	ld a3, 96(sp)
	ld a4, 104(sp)
	ld a5, 112(sp)
	ld a6, 120(sp)
	ld a7, 128(sp)
	ld s2, 136(sp)
	ld s3, 144(sp)
	ld s4, 152(sp)
	ld s5, 160(sp)
	ld s6, 168(sp)
	ld s7, 176(sp)
	ld s8, 184(sp)
	ld s9, 192(sp)
	ld s10, 200(sp)
	ld s11, 208(sp)
	ld t3, 216(sp)
	ld t4, 224(sp)
	ld t5, 232(sp)
	ld t6, 240(sp)

	addi sp, sp, 256

	mret
#endif

	csrrw a0, mscratch, a0
	sd a1, 0(a0)
	sd a2, 8(a0)
	sd a3, 16(a0)

	# schedule the next timer interrupt
	# by adding interval to mtimecmp.
	ld a1, 24(a0) # CLINT_MTIMECMP(hart)
	ld a2, 32(a0) # interval
	ld a3, 0(a1)
	add a3, a3, a2
	sd a3, 0(a1)

	# raise a supervisor software interrupt.
	li a1, 2
	csrw sip, a1

	ld a3, 16(a0)
	ld a2, 8(a0)
	ld a1, 0(a0)
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
	addi sp, sp, -256

	# save the registers.
	sd ra, 0(sp)
	sd sp, 8(sp)
	sd gp, 16(sp)
	sd tp, 24(sp)
	sd t0, 32(sp)
	sd t1, 40(sp)
	sd t2, 48(sp)
	sd s0, 56(sp)
	sd s1, 64(sp)
	sd a0, 72(sp)
	sd a1, 80(sp)
	sd a2, 88(sp)
	sd a3, 96(sp)
	sd a4, 104(sp)
	sd a5, 112(sp)
	sd a6, 120(sp)
	sd a7, 128(sp)
	sd s2, 136(sp)
	sd s3, 144(sp)
	sd s4, 152(sp)
	sd s5, 160(sp)
	sd s6, 168(sp)
	sd s7, 176(sp)
	sd s8, 184(sp)
	sd s9, 192(sp)
	sd s10, 200(sp)
	sd s11, 208(sp)
	sd t3, 216(sp)
	sd t4, 224(sp)
	sd t5, 232(sp)
	sd t6, 240(sp)

	csrr a0, sepc
	sd a0, 248(sp)


# call the C trap handler in trap.c
	mv a0, sp
	call kernel_trap

	# read sepc from the stack now, as we need a0 to be restored
	ld a0, 248(sp)
	csrw sepc, a0

	# restore registers.
	ld ra, 0(sp)
	ld sp, 8(sp)
	ld gp, 16(sp)
	# not this, in case we moved CPUs: ld tp, 24(sp)
	ld t0, 32(sp)
	ld t1, 40(sp)
	ld t2, 48(sp)
	ld s0, 56(sp)
	ld s1, 64(sp)
	ld a0, 72(sp)
	ld a1, 80(sp)
	ld a2, 88(sp)
	ld a3, 96(sp)
	ld a4, 104(sp)
	ld a5, 112(sp)
	ld a6, 120(sp)
	ld a7, 128(sp)
	ld s2, 136(sp)
	ld s3, 144(sp)
	ld s4, 152(sp)
	ld s5, 160(sp)
	ld s6, 168(sp)
	ld s7, 176(sp)
	ld s8, 184(sp)
	ld s9, 192(sp)
	ld s10, 200(sp)
	ld s11, 208(sp)
	ld t3, 216(sp)
	ld t4, 224(sp)
	ld t5, 232(sp)
	ld t6, 240(sp)

	addi sp, sp, 256

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
