#pragma once
#ifndef __ARCH_H__
#define __ARCH_H__


#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

/* On x86, import  */
#ifdef CONFIG_X86
#include "../../arch/x86/arch.h"
#endif




/*
 * registers are passed around in a cross-platform way using a reg_t*. The first
 * 6 of the entries are as follows: [0] = syscall code & return, [1...6] =
 * syscall arguments. Anything beyond that, like the PC, SP, BP etc... must be
 * accessed through arch:: funcs
 */
typedef unsigned long reg_t;


struct regs;
struct thread;

// Architecture specific functionality. Implemented in arch/$ARCH/*
namespace arch {


  // invalidate a page mapping
  void invalidate_page(unsigned long addr);

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
};    // namespace arch

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



#define REG_PC 0
#define REG_SP 1
#define REG_BP 2
#define REG_ARG0 3
#define REG_ARG1 4
// access a special register (only for PC, SP, BP)
reg_t &arch_reg(int ind, reg_t *);

void arch_disable_ints(void);
void arch_enable_ints(void);
void arch_relax(void);
void arch_halt();
void arch_mem_init(unsigned long mbd);
void arch_initialize_trapframe(bool userspace, reg_t *);
unsigned arch_trapframe_size(void);
void arch_dump_backtrace(void);
void arch_dispatch_function(void *func, long arg);
void arch_sigreturn(void);
void arch_flush_mmu(void);
void arch_save_fpu(struct thread &);
void arch_restore_fpu(struct thread &);
unsigned long arch_read_timestamp(void);

void serial_install();
int serial_rcvd(int device);
char serial_recv(int device);
char serial_recv_async(int device);
int serial_transmit_empty(int device);
void serial_send(int device, char out);
void serial_string(int device, char *out);


#endif
