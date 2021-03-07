#include <arch.h>
#include <cpu.h>
#include <riscv/setjmp.h>
#include <sched.h>
#include <ucontext.h>

#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define round_down(x, y) ((x) & ~((y)-1))
#define SIGSTKSZ 2 /* (pages) */

extern "C" void rv_enter_userspace(rv::regs *sp);


extern "C" void __rv_load_fpu(void *);
void arch_sigreturn(void *ucontext) {
  rv::regs *regs = (rv::regs *)curthd->trap_frame;
  if (!VALIDATE_RD(ucontext, sizeof(*regs))) {
    curproc->terminate(SIGSEGV);
  }

  auto *uctx = (struct ucontext *)ucontext;

  memcpy(regs, ucontext, 32 * sizeof(rv::xsize_t));
  __rv_load_fpu(uctx->fpu);
  /* jump to returning to userspace :^) */
  rv_enter_userspace(regs);
}
