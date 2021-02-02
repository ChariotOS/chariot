# lowlevel.S - All low level functionality (boot and util)
# riscv64 bootloader for ChariotOS
# Nick Wanninger
# 29 December, 2020
#include <asmdefs.h>


/* Status register flags */
#define SR_SIE		(0x00000002UL) /* Supervisor Interrupt Enable */
#define SR_MIE		(0x00000008UL) /* Machine Interrupt Enable */
#define SR_SPIE		(0x00000020UL) /* Previous Supervisor IE */
#define SR_MPIE		(0x00000080UL) /* Previous Machine IE */
#define SR_SPP		(0x00000100UL) /* Previously Supervisor */
#define SR_MPP		(0x00001800UL) /* Previously Machine */
#define SR_SUM		(0x00040000UL) /* Supervisor User Memory Access */

#define CSR_SSTATUS		0x100
#define CSR_SIE			0x104
#define CSR_STVEC		0x105
#define CSR_SCOUNTEREN		0x106
#define CSR_SSCRATCH		0x140
#define CSR_SEPC		0x141
#define CSR_SCAUSE		0x142
#define CSR_STVAL		0x143
#define CSR_SIP			0x144
#define CSR_SATP		0x180




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











#define BUFFER_SPACE 0

# When the processor gets an interrupt or traps, it jumps here and sets some CSRs
# like sstatus, sbadaddr, stval etc... The only problem is that we don't switch
# stacks when jumping from userspace. To fix this, we gotta do some extra nonsense
# to avoid losing register states
#define TP_BAK1 0
#define TP_BAK2 1
#define TP_BAK3 2
#define TP_TCA  3
#define TP_INTERVAL  4
#define TP_KERNEL_SP 5
#define TP_USER_SP 6
#define TF_SIZE_ON_STACK (36 * SZREG)

ENTRY(kernelvec)

	/*
	 * When coming from userspace, sscratch's value will be that of the hart_state for this
	 * hart. If it is zero, the we are coming from kernelspace. 
	 */

	csrrw tp, sscratch, tp
	bnez tp, _save_context

_restore_kernel_tpsp: # sscratch != 0
	csrr tp, sscratch # tp <- sscratch
	REG_S sp, ROFF(TP_KERNEL_SP, tp) # sp <- tp.kernel_sp
_save_context:
	REG_S sp, ROFF(TP_USER_SP, tp)
	REG_L sp, ROFF(TP_KERNEL_SP, tp)
	addi sp, sp, -(TF_SIZE_ON_STACK)

	# save the registers.
	REG_S ra, ROFF(0, sp)
	# not sp
	REG_S gp, ROFF(2, sp)
	# not tp
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


	# TODO: disable floating point in the kernel
	li t0, 0 # TODO: these flags :)

	REG_L s0, ROFF(TP_USER_SP, tp)
	csrrc s1, sstatus, t0
	csrr s2, sepc
	csrr s3, stval
	csrr s4, scause
	csrr s5, sscratch # sscratch is currently the old tp value

	REG_S s0, ROFF(1, sp) # store previous sp in the trapframe
	REG_S s1, ROFF(32, sp) # store sstatus in the trapframe
	REG_S s2, ROFF(31, sp) # store sepc in the trapframe
	REG_S s3, ROFF(33, sp)
	REG_S s4, ROFF(34, sp)
	REG_S s5, ROFF(3, sp) # store previous tp value in the trapframe


	/*
	 * Set the scratch register to 0, so that if a recursive exception
	 * occurs, the exception vector knows it came from the kernel
	 */
	csrw sscratch, zero

	# TODO: directly handle syscalls to save some cycles? idk :)

	/* Return  */
	la ra, ret_from_exception

	/* Handle interrupts */
	move a0, sp /* arg0 <- trapframe */
	j kernel_trap


ret_from_exception:

	REG_L s0, ROFF(32, sp) # s0 <- tf.status
	csrc sstatus, SR_SIE

	# check if we were previously in supervisor mode
	andi s0, s0, SR_SPP
	bnez s0, restore_all # skip "resume_userspace" if we were in the kernel

resume_userspace:
	/* Save unwound kernel stack pointer in thread_info */
	addi s0, sp, TF_SIZE_ON_STACK
	REG_S s0, ROFF(TP_KERNEL_SP, tp) # tf.kernel_sp <- sp

	/*
	 * Save TP into the scratch register , so we can find the kernel data
	 * structures again.
	 */
	csrw sscratch, tp

restore_all:
	
	REG_L a0, ROFF(32, sp) # a0 <- tf.status
	REG_L  a2, ROFF(31, sp) # a2 <- tf.sepc
	# REG_SC x0, a2, ROFF(31, sp) # ngl, idk what REG_SC does...
	
	csrw sstatus, a0
	csrw sepc, a2

	# restore registers
	REG_L ra, ROFF(0, sp)
  # skip sp for now...
	REG_L gp, ROFF(2, sp)
	REG_S tp, ROFF(3, sp)
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

	# NOW we load sp
	REG_L sp, ROFF(1, sp)

	# finally return the userspace
	sret


END(kernelvec)










ENTRY(rv_enter_userspace)
	# simply change sp <- a0 and jump to ret_from_exception
	move sp, a0
	j ret_from_exception



END(rv_enter_userspace)

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
