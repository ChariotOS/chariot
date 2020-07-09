#include <arch.h>
#include <cpu.h>
#include <phys.h>
#include "setjmp.h"

#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

struct sig_priv {
  jmp_buf jb;
};


void arch::sigreturn(void) {

  assert(curthd->sig.arch_priv != NULL);
  auto priv = static_cast<struct sig_priv *>(curthd->sig.arch_priv);
  longjmp(priv->jb, 1);  // jump back with a 1 code
}
#define round_down(x, y) ((x) & ~((y)-1))
#define SIGSTKSZ 1 /* (pages) */

extern "C" void jmp_to_userspace(uint64_t ip, uint64_t sp, uint64_t a, uint64_t b, uint64_t c);
void arch::dispatch_signal(int sig) {
  if (curthd->sig.arch_priv == NULL) {
    curthd->sig.arch_priv = (void *)new sig_priv();
  }

  auto priv = static_cast<struct sig_priv *>(curthd->sig.arch_priv);
  // int thing = 0xFFAADDEE;

  auto &handler = curproc->sig.handlers[sig];


  if (handler.sa_handler == SIG_IGN) {
    return;
  }
  if (handler.sa_handler == SIG_DFL) {
    switch (sig) {
      case SIGCHLD:
      case SIGURG:
      case SIGWINCH:
        return;
      default:
        panic("KILL PROC WITH %d\n", sig);
        // kill_process(running_process, signal + 256);
    }
  }

  void *sigstk = phys::kalloc(SIGSTKSZ);

  void *old_stack = curthd->stack;
  long old_stack_size = curthd->stack_size;
  uint64_t old_sp = arch::reg(REG_SP, curthd->trap_frame);

  // printk("old stack: %p\n", old_stack);
  if (setjmp(priv->jb)) {
    // printk("GOT BACK TO DISPATCH_SIGNAL %08x\n", thing);
    curthd->stack = old_stack;
    curthd->stack_size = old_stack_size;

		cpu::switch_vm(curthd);
    phys::kfree(sigstk, SIGSTKSZ);
    return;
  }



  uint64_t new_sp = round_down(old_sp - 128, 16);

  uint64_t *pnew_sp = (uint64_t *)new_sp;
  pnew_sp[0] = curproc->sig.ret;  // rbp + 8
  pnew_sp[1] = 0;                 // rbp
	// printk("new sp: %p -> %p. Handler: %p\n", old_sp, new_sp, handler.sa_handler);


	arch::cli();
  curthd->stack_size = SIGSTKSZ * PGSIZE;
  curthd->stack = (void*)((off_t)sigstk + curthd->stack_size);
	cpu::switch_vm(curthd);
	arch::sti();

  jmp_to_userspace((uint64_t)handler.sa_handler, new_sp, sig, 0, 0);

	panic("oh no, got back here.\n");
}
