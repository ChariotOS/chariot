#include <cpu.h>
#include <elf/loader.h>
#include <pctl.h>
#include <syscall.h>



static int do_create_thread(struct pctl_create_thread_args *argp) {
  return -1;
}

int sys::pctl(int pid, int request, u64 arg) {

	auto rbp = arch::reg(REG_BP, curthd->trap_frame);
	auto rip = arch::reg(REG_PC, curthd->trap_frame);
  if (cpu::in_thread() && curproc->ring == RING_USER) {
    auto bt = curthd->backtrace(rbp, rip);
    cpu::pushcli();
    printk_nolock("backtrace: ");
    for (auto rip : bt) {
      printk_nolock("%p ", rip);
    }
    printk_nolock("\n");
    cpu::popcli();
  }
	return 0;


  // printk("pctl(%d, %d, %p);\n", pid, request, arg);
  switch (request) {
    case PCTL_CREATE_THREAD:
      return do_create_thread((struct pctl_create_thread_args *)arg);

    // request not handled!
    default:
      return -1;
  }
  return -1;
}
