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

void arch_sigreturn(void) {
  assert(curthd->sig.arch_priv != NULL);
  auto priv = static_cast<struct sig_priv *>(curthd->sig.arch_priv);
  longjmp(priv->jb, 1);  // jump back with a 1 code
}


void arch_dispatch_function(void *func, long arg) {
  if (curthd->sig.arch_priv == NULL) {
    curthd->sig.arch_priv = (void *)new sig_priv();
  }

  auto priv = static_cast<struct sig_priv *>(curthd->sig.arch_priv);

  if (setjmp(priv->jb)) {
    cpu::switch_vm(curthd);
    return;
  }


  arch_disable_ints();
  cpu::switch_vm(curthd);
  arch_enable_ints();

  rv::xsize_t old_sp = curthd->userspace_sp;
  /* not sure we need to do this in riscv. There is no red zone */
  rv::xsize_t new_sp = round_down(old_sp - 32, 16);

  rv::regs regs = {0};

  regs.status = read_csr(sstatus) & ~SSTATUS_SPP;
  regs.sepc = (rv::xsize_t)func;
  regs.a0 = (rv::xsize_t)arg;
  regs.ra = curproc->sig.ret;
  regs.sp = new_sp;

  rv_enter_userspace(&regs);

  panic("oh no, got back here.\n");

  printk("dispatch function 0x%P with arg 0x%llx (%ld)\n", func, arg, arg);
}
