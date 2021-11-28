#pragma once

#include <types.h>


// cpsr/spsr bits
#define NO_INT 0xc0
#define DIS_INT 0x80

namespace arm64 {

  using reg_t = uint64_t;

  struct regs {
    arm64::reg_t sp;  // user mode sp
    arm64::reg_t pc;  // user mode pc (elr)
    arm64::reg_t spsr;
    arm64::reg_t r0;
    arm64::reg_t r1;
    arm64::reg_t r2;
    arm64::reg_t r3;
    arm64::reg_t r4;
    arm64::reg_t r5;
    arm64::reg_t r6;
    arm64::reg_t r7;
    arm64::reg_t r8;
    arm64::reg_t r9;
    arm64::reg_t r10;
    arm64::reg_t r11;
    arm64::reg_t r12;
    arm64::reg_t r13;
    arm64::reg_t r14;
    arm64::reg_t r15;
    arm64::reg_t r16;
    arm64::reg_t r17;
    arm64::reg_t r18;
    arm64::reg_t r19;
    arm64::reg_t r20;
    arm64::reg_t r21;
    arm64::reg_t r22;
    arm64::reg_t r23;
    arm64::reg_t r24;
    arm64::reg_t r25;
    arm64::reg_t r26;
    arm64::reg_t r27;
    arm64::reg_t r28;
    arm64::reg_t r29;
    arm64::reg_t r30;  // user mode lr
  };




	// mmio memory operations
  inline void write(long reg, uint32_t val) { *(volatile uint32_t *)reg = val; }
  inline uint32_t read(long reg) { return *(volatile uint32_t *)reg; }


  struct MMURegion {
    off_t start;
    size_t size;
    int flags;
    const char *name;
  };

  extern MMURegion mmu_mappings[];

  extern void platform_init(uint64_t dtb, uint64_t x1, uint64_t x2, uint64_t x3);
};  // namespace arm64
