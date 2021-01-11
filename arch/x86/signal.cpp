#include <arch.h>
#include <cpu.h>
#include <phys.h>
#include <x86/setjmp.h>

#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

struct sig_priv {
  jmp_buf jb;
};


void arch_sigreturn(void) {

  assert(curthd->sig.arch_priv != NULL);
  auto priv = static_cast<struct sig_priv *>(curthd->sig.arch_priv);
  longjmp(priv->jb, 1);  // jump back with a 1 code
}
#define round_down(x, y) ((x) & ~((y)-1))
#define SIGSTKSZ 2 /* (pages) */

extern "C" void jmp_to_userspace(uint64_t ip, uint64_t sp, uint64_t a, uint64_t b, uint64_t c);
void arch_dispatch_function(void *func, long arg) {
  if (curthd->sig.arch_priv == NULL) {
    curthd->sig.arch_priv = (void *)new sig_priv();
  }

  auto priv = static_cast<struct sig_priv *>(curthd->sig.arch_priv);
  // int thing = 0xFFAADDEE;

  // printk("old stack: %p\n", old_stack);
  if (setjmp(priv->jb)) {

		auto s = curthd->stacks.take_last();
		free(s.start);

		cpu::switch_vm(curthd);

    return;
  }


  uint64_t old_sp = arch_reg(REG_SP, curthd->trap_frame);
  uint64_t new_sp = round_down(old_sp - 128, 16);

  uint64_t *pnew_sp = (uint64_t *)new_sp;
  pnew_sp[0] = curproc->sig.ret;  // rbp + 8
  pnew_sp[1] = 0;                 // rbp
	// printk("new sp: %p -> %p. Handler: %p\n", old_sp, new_sp, handler.sa_handler);


	struct thread::kernel_stack s;
	s.size = SIGSTKSZ * PGSIZE;
  s.start = (void*)malloc(s.size);
	curthd->stacks.push(move(s));

	arch_disable_ints();
	cpu::switch_vm(curthd);
	arch_enable_ints();

  jmp_to_userspace((uint64_t)func, new_sp, arg, 0, 0);

	panic("oh no, got back here.\n");
}
