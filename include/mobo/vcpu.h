#pragma once

#ifndef __MOBO_VCPU_
#define __MOBO_VCPU_

#include <mobo/types.h>
#include <functional>
#include <vector>

namespace mobo {

#define NR_INTERRUPTS 256

// general purpose registers
struct regs {
  u64 rax, rbx, rcx, rdx;
  u64 rsi, rdi, rsp, rbp;
  u64 r8, r9, r10, r11;
  u64 r12, r13, r14, r15;
  u64 rip, rflags;
};

// memory segmentation information
struct segment {
  u64 base;
  u32 limit;
  u16 selector;
  u8 type;
  u8 present, dpl, db, s, l, g, avl;
  u8 unusable;
};

struct dtable {
  u64 base;
  u16 limit;
};

// special purpose registers
struct sregs {
  /* out (KVM_GET_SREGS) / in (KVM_SET_SREGS) */
  struct segment cs, ds, es, fs, gs, ss;
  struct segment tr, ldt;
  struct dtable gdt, idt;
  u64 cr0, cr2, cr3, cr4, cr8;
  u64 efer;
  u64 apic_base;
  u64 interrupt_bitmap[(NR_INTERRUPTS + 63) / 64];
};




// Floating point registers
struct fpu_regs {
	u8  fpr[8][16];
	u16 fcw;
	u16 fsw;
	u8  ftwx;  /* in fxsave format */
	u8  pad1;
	u16 last_opcode;
	u64 last_ip;
	u64 last_dp;
	u8  xmm[16][16];
	u32 mxcsr;
	u32 pad2;
};


// the vcpu struct is a general interface to a virtualized x86_64 CPU
class vcpu {
 public:
  // GPR
  virtual void read_regs(regs&) = 0;
  virtual void write_regs(regs&) = 0;
  // SPR
  virtual void read_sregs(sregs&) = 0;
  virtual void write_sregs(sregs&) = 0;
  // FPR
  virtual void read_fregs(fpu_regs&) = 0;
  virtual void write_fregs(fpu_regs&) = 0;
};

};  // namespace mobo

#endif
