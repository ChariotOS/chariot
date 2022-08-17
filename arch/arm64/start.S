
#include <asmdefs.h>

# start.S - Boot level functionality for ChariotOS on arm64

# AArch64 mode
 
# To keep this in the first portion of the binary.
.section .init.text
 
# Make _start global.
.globl _start
# Entry point for the kernel. Registers:
# x0 -> 32 bit pointer to DTB in memory (primary core only) / 0 (secondary cores)
# x1 -> 0
# x2 -> 0
# x3 -> 0
# x4 -> 32 bit kernel entry point, _start location


tmp                     .req x9

_start:
    # Check processor ID is zero (executing on main core), else hang
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbz     x1, 2f

    /* enable caches so atomics and spinlocks work */
    mrs     tmp, sctlr_el1
    orr     tmp, tmp, #(1<<12) /* Enable icache */
    orr     tmp, tmp, #(1<<2)  /* Enable dcache/ucache */
    orr     tmp, tmp, #(1<<3)  /* Enable Stack Alignment Check EL1 */
    orr     tmp, tmp, #(1<<4)  /* Enable Stack Alignment Check EL0 */
    bic     tmp, tmp, #(1<<1)  /* Disable Alignment Checking for EL1 EL0 */
    msr     sctlr_el1, tmp

    # We're not on the main core, so hang in an infinite wait loop
1:  wfe
    b       1b


2:  # We're on the main core!

    # Set stack to start below our code
    ldr     x1, =boot_stack_end
    mov     sp, x1

    # Clean the BSS section
    ldr     x1, =__bss_start
    ldr     x2, =__bss_size
3:  cbz     x2, 4f
    str     xzr, [x1], #8
    sub     x2, x2, #1
    cbnz    x2, 3b

    # Jump to our kernel_main routine in C (make sure it doesn't return)
4:  bl      kernel_main
    # In case it does return, halt the master core too
    b       1b


.section .init.data

.align 16
boot_stack:
	.space 4096
boot_stack_end:
