#include <arch.h>
#include <sched.h>
#include <cpu.h>



#define SIG_ERR ((void (*)(int)) - 1)
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

struct sig_priv {
  // jmp_buf jb;
};


void arch_dispatch_function(void *func, long arg) {

}
void arch_sigreturn(void) {
  assert(curthd->sig.arch_priv != NULL);
  auto priv = static_cast<struct sig_priv *>(curthd->sig.arch_priv);
  // longjmp(priv->jb, 1);  // jump back with a 1 code
}
