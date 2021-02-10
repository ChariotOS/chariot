# lowlevel.S - All low level functionality (boot and util)
# riscv64 bootloader for ChariotOS
# Nick Wanninger
# 29 December, 2020
#include <asmdefs.h>
#include <riscv/paging.h>

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


#define MSTATUS_MPP_MASK (3L << 11)  // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)  // machine-mode interrupt enable.


#define SIE_SEIE (1L << 9)  // external
#define SIE_STIE (1L << 5)  // timer
#define SIE_SSIE (1L << 1)  // software

#define MIE_MEIE (1L << 11)  // external
#define MIE_MTIE (1L << 7)  // timer
#define MIE_MSIE (1L << 3)  // software


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


	csrr    t0, mhartid
0:
	bne     t0, zero, 0b

	# we calculate the starting stack by _stack_start + (2*4096 * (mhartid + 1))
	# so each HART gets 2 pages of stack
	la      sp, _stack_start
	li      t0, CONFIG_RISCV_BOOTSTACK_SIZE * 4096 # 2 page stack
	csrr    t1, mhartid
	addi    t1, t1, 1 # t1 = mhartid + 1
	mul     t0, t0, t1 # t1 = SIZE * (mhartid + 1)
	add     sp, sp, t0 # sp = stack_start + (SIZE * (mhartid + 1))

	j 1f
	lla     t0, _bss_start
	lla     t1, _bss_end
	beq     t0, t1, 1f
0:
	REG_S   zero, (t0)
	add     t0, t0, SZREG
	blt     t1, t0, 0b  # if t1 is less than t0 
1:


	# setup the base high-half identity mapping
	li      t0, PT_R | PT_W | PT_X | PT_V # t0: current physical address (PTE)
	la      t1, kernel_page_table         # t1: current location in the page table

	li      t3, 0x1000
	add     t2, t1, t3                    # t2: address after the page table (the end)


	# shift the current pointer to the halfway point
	srai    t3, t3, 1                     # t3 /= 2
	add     t1, t1, t3                    # t1 += 4096 / 2

	li      t3, VM_BIGGEST_PAGE           # t3: physical memory stride
0:

	# store the PTE at the location pointed to by t1
	REG_S   t0, 0(t1)
	# move the PTE (t0) forward by the stride (t3)
	add     t0, t0, t3
	# step the pointer forward
	addi    t1, t1, SZREG
	bne     t1, t2, 0b                    # jump back if we aren't done



	# now, load the kernel page table into satp 
	la      t0, kernel_page_table
	srai    t0, t0, 12 # shift the page table to be just the PPN

	# t1 is the eventual value of SATP
	li      t1, SATP_FLAG      # t1 = SATP_FLAG
	slli    t1, t1, SATP_SHIFT # t1 = t1 << SATP_SHIFT

	add     t1, t1, t0         # t1 = t1 + kernel_page_table

	csrw    satp, t1           # satp = t1

	sfence.vma  zero, zero



	# set M-mode trap handler
	la      t0, timervec
	csrw    mtvec, t0

	# setup trap delegation
	li      t0, 0xffffffff
	csrw    medeleg, t0
	csrw    mideleg, t0


	# set M-mode previous priv mode to S-mode for mret
	li      t0, MSTATUS_MPP_MASK
	csrc    mstatus, t0 # mstatus.MPP = 0
	li      t0, MSTATUS_MPP_S
	csrs    mstatus, t0 # mstatus.MPP = S

	# set mepc so we can return to the right place
	la      t0, main_ptr
	REG_L   t0, 0(t0)
	csrw    mepc, t0

	# allocate alot of space for the scratch pad
	addi    sp, sp, -32 * SZREG

	csrw    mscratch, sp

	# ask C code to setup the local hart state, cause it's annoying to work with structs in asm
	move    a0, sp
	# a1 is still the pointer to the device tree binary
	call    setup_mhart_state


	
	# enable machine mode interrupts
	li      t0, MSTATUS_MIE
	csrs    mstatus, t0

	# enable machine mode timer interrupts
	li      t0, MIE_MTIE
	csrs    mie, t0

	li      t0, SIE_SEIE | SIE_STIE | SIE_SSIE
	csrs    sie, t0
	
	move    tp, sp # tp is how we communicate with the supervisor mode

	la      t0, virt_mask
	REG_L   t0, 0(t0)
	add     sp, sp, t0 # load the new virtual stack pointer

	mret


.balign 8
main_ptr:
.quad main
# place the kernel virtual base here, so we don't have to manage calculation in assembly
virt_mask:
.quad CONFIG_KERNEL_VIRTUAL_BASE


.global timervec
timervec:
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











.section .text









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




/* _setjmp functions */
ENTRY (_setjmp)
  li	a1, 0
  j	__sigsetjmp
END (_setjmp)
ENTRY (setjmp)
  li	a1, 1
  /* Fallthrough */
END (setjmp)
ENTRY (__sigsetjmp)
	REG_S ra,  0*SZREG(a0)
	REG_S s0,  1*SZREG(a0)
	REG_S s1,  2*SZREG(a0)
	REG_S s2,  3*SZREG(a0)
	REG_S s3,  4*SZREG(a0)
	REG_S s4,  5*SZREG(a0)
	REG_S s5,  6*SZREG(a0)
	REG_S s6,  7*SZREG(a0)
	REG_S s7,  8*SZREG(a0)
	REG_S s8,  9*SZREG(a0)
	REG_S s9, 10*SZREG(a0)
	REG_S s10,11*SZREG(a0)
	REG_S s11,12*SZREG(a0)
	REG_S sp, 13*SZREG(a0)

#ifndef __riscv_float_abi_soft
	FREG_S fs0, 14*SZREG+ 0*SZFREG(a0)
	FREG_S fs1, 14*SZREG+ 1*SZFREG(a0)
	FREG_S fs2, 14*SZREG+ 2*SZFREG(a0)
	FREG_S fs3, 14*SZREG+ 3*SZFREG(a0)
	FREG_S fs4, 14*SZREG+ 4*SZFREG(a0)
	FREG_S fs5, 14*SZREG+ 5*SZFREG(a0)
	FREG_S fs6, 14*SZREG+ 6*SZFREG(a0)
	FREG_S fs7, 14*SZREG+ 7*SZFREG(a0)
	FREG_S fs8, 14*SZREG+ 8*SZFREG(a0)
	FREG_S fs9, 14*SZREG+ 9*SZFREG(a0)
	FREG_S fs10,14*SZREG+10*SZFREG(a0)
	FREG_S fs11,14*SZREG+11*SZFREG(a0)
#endif

/* TODO: save signal mask? */
  li a0, 0
  ret


END (__sigsetjmp)

ENTRY (longjmp)
	REG_L ra,  0*SZREG(a0)
	REG_L s0,  1*SZREG(a0)
	REG_L s1,  2*SZREG(a0)
	REG_L s2,  3*SZREG(a0)
	REG_L s3,  4*SZREG(a0)
	REG_L s4,  5*SZREG(a0)
	REG_L s5,  6*SZREG(a0)
	REG_L s6,  7*SZREG(a0)
	REG_L s7,  8*SZREG(a0)
	REG_L s8,  9*SZREG(a0)
	REG_L s9, 10*SZREG(a0)
	REG_L s10,11*SZREG(a0)
	REG_L s11,12*SZREG(a0)
	REG_L sp, 13*SZREG(a0)

#ifndef __riscv_float_abi_soft
	FREG_L fs0, 14*SZREG+ 0*SZFREG(a0)
	FREG_L fs1, 14*SZREG+ 1*SZFREG(a0)
	FREG_L fs2, 14*SZREG+ 2*SZFREG(a0)
	FREG_L fs3, 14*SZREG+ 3*SZFREG(a0)
	FREG_L fs4, 14*SZREG+ 4*SZFREG(a0)
	FREG_L fs5, 14*SZREG+ 5*SZFREG(a0)
	FREG_L fs6, 14*SZREG+ 6*SZFREG(a0)
	FREG_L fs7, 14*SZREG+ 7*SZFREG(a0)
	FREG_L fs8, 14*SZREG+ 8*SZFREG(a0)
	FREG_L fs9, 14*SZREG+ 9*SZFREG(a0)
	FREG_L fs10,14*SZREG+10*SZFREG(a0)
	FREG_L fs11,14*SZREG+11*SZFREG(a0)
#endif

	seqz a0, a1
	add  a0, a0, a1   # a0 = (a1 == 0) ? 1 : a1
	ret

END (longjmp)




ENTRY(__rv_save_fpu)
	fsd f0,  8 * 0(a0)
	fsd f1,  8 * 1(a0)
	fsd f2,  8 * 2(a0)
	fsd f3,  8 * 3(a0)
	fsd f4,  8 * 4(a0)
	fsd f5,  8 * 5(a0)
	fsd f6,  8 * 6(a0)
	fsd f7,  8 * 7(a0)
	fsd f8,  8 * 8(a0)
	fsd f9,  8 * 9(a0)
	fsd f10, 8 * 10(a0)
	fsd f11, 8 * 11(a0)
	fsd f12, 8 * 12(a0)
	fsd f13, 8 * 13(a0)
	fsd f14, 8 * 14(a0)
	fsd f15, 8 * 15(a0)
	fsd f16, 8 * 16(a0)
	fsd f17, 8 * 17(a0)
	fsd f18, 8 * 18(a0)
	fsd f19, 8 * 19(a0)
	fsd f20, 8 * 20(a0)
	fsd f21, 8 * 21(a0)
	fsd f22, 8 * 22(a0)
	fsd f23, 8 * 23(a0)
	fsd f24, 8 * 24(a0)
	fsd f25, 8 * 25(a0)
	fsd f26, 8 * 26(a0)
	fsd f27, 8 * 27(a0)
	fsd f28, 8 * 28(a0)
	fsd f29, 8 * 29(a0)
	fsd f30, 8 * 30(a0)
	fsd f31, 8 * 31(a0)
	ret
END(__rv_save_fpu)

ENTRY(__rv_load_fpu)
	fld f0,  8 * 0(a0)
	fld f1,  8 * 1(a0)
	fld f2,  8 * 2(a0)
	fld f3,  8 * 3(a0)
	fld f4,  8 * 4(a0)
	fld f5,  8 * 5(a0)
	fld f6,  8 * 6(a0)
	fld f7,  8 * 7(a0)
	fld f8,  8 * 8(a0)
	fld f9,  8 * 9(a0)
	fld f10, 8 * 10(a0)
	fld f11, 8 * 11(a0)
	fld f12, 8 * 12(a0)
	fld f13, 8 * 13(a0)
	fld f14, 8 * 14(a0)
	fld f15, 8 * 15(a0)
	fld f16, 8 * 16(a0)
	fld f17, 8 * 17(a0)
	fld f18, 8 * 18(a0)
	fld f19, 8 * 19(a0)
	fld f20, 8 * 20(a0)
	fld f21, 8 * 21(a0)
	fld f22, 8 * 22(a0)
	fld f23, 8 * 23(a0)
	fld f24, 8 * 24(a0)
	fld f25, 8 * 25(a0)
	fld f26, 8 * 26(a0)
	fld f27, 8 * 27(a0)
	fld f28, 8 * 28(a0)
	fld f29, 8 * 29(a0)
	fld f30, 8 * 30(a0)
	fld f31, 8 * 31(a0)
	ret

END(__rv_load_fpu)
