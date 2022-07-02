#pragma once
#ifndef __ARCH_H__
#define __ARCH_H__


#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

/* On x86, import  */
#ifdef CONFIG_X86
#include <x86/arch.h>
#endif

#include <types.h>

#include <fwd.h>

#ifdef CONFIG_RISCV
/* not sure if we need this yet or not... */
// #include <riscv/arch.h>
#endif



/*
 * registers are passed around in a cross-platform way using a reg_t*. The first
 * 6 of the entries are as follows: [0] = syscall code & return, [1...6] =
 * syscall arguments. Anything beyond that, like the PC, SP, BP etc... must be
 * accessed through arch:: funcs
 */
#ifdef CONFIG_64BIT
typedef uint64_t reg_t;
#else
typedef uint32_t reg_t;
#endif



struct regs;

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

  using handler = void (*)(int i, reg_t *, void *data);

  // install and remove handlers
  int install(int irq, irq::handler handler, const char *name, void *data = 0);
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
bool arch_irqs_enabled(void);
void arch_relax(void);
void arch_halt();
void arch_mem_init(unsigned long mbd);

void arch_initialize_trapframe(bool userspace, reg_t *);
unsigned arch_trapframe_size(void);
void arch_dump_backtrace(void);
void arch_dispatch_signal(int sig, void *handler, void *ucontext);


int arch_generate_backtrace(off_t virt_ebp, off_t *buf, size_t bufsz);

void arch_sigreturn(void *ucontext);
void arch_flush_mmu(void);
void arch_save_fpu(struct Thread &);
void arch_restore_fpu(struct Thread &);
unsigned long arch_read_timestamp(void);


// convert timestamp or nanoseconds to eachother
unsigned long arch_timestamp_to_ns(unsigned long ts);
unsigned long arch_ns_to_timestamp(unsigned long ns);

/* get the current second since boot time */
unsigned long arch_seconds_since_boot(void);

void arch_thread_create_callback();

// Set a timer interrupt for `nanos` nanoseconds in the future
int arch_set_timer(uint64_t nanos);
int arch_stop_timer();

void serial_install();
int serial_rcvd(int device);
char serial_recv(int device);
char serial_recv_async(int device);
int serial_transmit_empty(int device);
void serial_send(int device, char out);
void serial_string(int device, char *out);




// fire off a xcall, and do not wait. This is handled by the scheduler
// if `core == -1`, call to all cores
void arch_deliver_xcall(int core);




#endif
