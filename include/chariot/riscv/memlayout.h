#include <mem.h>

#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1



#define MREG(r) (*(uint32_t*)(r))

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC ((rv::xsize_t)p2v(0x0c000000L))
#define PLIC_PRIORITY MREG(PLIC + 0x0)
#define PLIC_PENDING MREG(PLIC + 0x1000)
#define PLIC_MENABLE(hart) MREG(PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) MREG(PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) MREG(PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) MREG(PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) MREG(PLI + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) MREG(PLIC + 0x201004 + (hart)*0x2000)

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + CONFIG_RISCV_RAM_MB * 1024LU * 1024LU)

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * 2 * PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
