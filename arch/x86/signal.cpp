#include <arch.h>
#include <cpu.h>
#include <phys.h>
#include <util.h>
#include <x86/setjmp.h>

#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

struct sig_priv {
  jmp_buf jb;
};

extern "C" void x86_enter_userspace(x86_64regs *);


void arch_sigreturn(void *ucontext) {
  int frame_size = sizeof(x86_64regs);
  auto *regs = (struct x86_64regs *)curthd->trap_frame;
  if (!VALIDATE_RD(ucontext, frame_size)) curproc->terminate(SIGSEGV);

  /* back these up. We don't want the user to be able to change these :) */
  auto cs = regs->cs;
  auto ds = regs->ds;

  memcpy(regs, ucontext, frame_size);

  regs->cs = cs;
  regs->ds = ds;
  /* jump to returning to userspace :^) */
  x86_enter_userspace(regs);
}

#define round_down(x, y) ((x) & ~((y)-1))
#define SIGSTKSZ 2 /* (pages) */

