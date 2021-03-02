#include <arch.h>
#include <cpu.h>
#include <riscv/setjmp.h>
#include <sched.h>

#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define round_down(x, y) ((x) & ~((y)-1))
#define SIGSTKSZ 2 /* (pages) */

struct sig_priv {
  jmp_buf jb;
};


extern "C" void rv_enter_userspace(rv::regs *sp);

void arch_sigreturn(void *ucontext) {
  printk("ucontext sigreturn %p\n", ucontext);

  rv::regs *regs = (rv::regs*)curthd->trap_frame;

  if (!VALIDATE_RD(ucontext, sizeof(*regs))) {
    curproc->terminate(SIGSEGV);
  }

	memcpy(regs, ucontext, sizeof(*regs));
	/* jump to returning to userspace :^) */
	rv_enter_userspace(regs);
}

