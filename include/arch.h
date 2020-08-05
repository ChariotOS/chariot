#pragma once
#ifndef __ARCH_H__
#define __ARCH_H__


#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

/*
 * registers are passed around in a cross-platform way using a reg_t*. The first
 * 6 of the entries are as follows: [0] = syscall code & return, [1...6] =
 * syscall arguments. Anything beyond that, like the PC, SP, BP etc... must be
 * accessed through arch:: funcs
 */
typedef unsigned long reg_t;


struct regs;

// Architecture specific functionality. Implemented in arch/$ARCH/*
namespace arch {


#define REG_PC 0
#define REG_SP 1
#define REG_BP 2
// access a special register (only for PC, SP, BP)
reg_t &reg(int ind, reg_t *);
// allows allocation of a trapframe in thread.cpp
unsigned trapframe_size(void);
// setup the trapframe in a way that allows returning to userspace
// or the kernel
void initialize_trapframe(bool userspace, reg_t *);


void cli(void);
void sti(void);

void halt(void);

// invalidate a page mapping
void invalidate_page(unsigned long addr);

void flush_tlb(void);

// initialize the physical memory map from a multiboot record
void mem_init(unsigned long mbd);

unsigned long read_timestamp(void);


// use whatever the arch can use to get a high accuracy reading of time.
unsigned long us_this_second(void);

// dispatch a signal to userspace and return when done
void dispatch_signal(int sig);
void sigreturn(void);



void dump_backtrace(void);
/**
 * the architecture must only implement init() and eoi(). The arch must
 * implement calling irq::dispatch() when an interrupt is received.
 */
namespace irq {
// init architecture specific interrupt logic. Implemented by arch
int init(void);
// End of Interrupt.
void eoi(int i);

// enable or disable interrupts
void enable(int num);
void disable(int num);
};  // namespace irq
};  // namespace arch

// core kernel irq (implemented in src/irq.cpp)
namespace irq {

using handler = void (*)(int i, reg_t *);

// install and remove handlers
int install(int irq, irq::handler handler, const char *name);
irq::handler uninstall(int irq);

int init(void);
inline void eoi(int i) { arch::irq::eoi(i); }

// cause an interrupt to be handled by the kernel's interrupt dispatcher
void dispatch(int irq, reg_t *);



};  // namespace irq

#endif
