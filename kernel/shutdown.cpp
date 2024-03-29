#include <asm.h>
#include <fs.h>
#include <syscall.h>
#include <module.h>
#include <kshell.h>
#ifdef CONFIG_RISCV
#include <riscv/sbi.h>
#endif

int sys::shutdown(void) {
  printf(KERN_WARN "Shutting down...\n");
  printf(KERN_WARN "Flushing block cache\n");
  block::sync_all();
  /* TODO: kill everyone! */

#ifdef CONFIG_X86

  printf(KERN_WARN "Trying BOCHS/Old-Qemu method\n");
  /* Bochs and old QEMU */
  outw(0xB004, 0x2000);
  printf(KERN_WARN "Trying new QEMU method\n");
  /* Newer QEMU */
  outw(0x604, 0x2000);
  printf(KERN_WARN "Trying VirtualBox method\n");
  /* Virtual box */
  outw(0x4004, 0x3400);

  printf(KERN_WARN "Trying QEMU isa-debug-exit method\n");
  outb(0x501, 0x00);

#endif


#if defined(CONFIG_RISCV)
  sbi_call(SBI_SHUTDOWN);
#endif
  printf(KERN_ERROR "Failed to shutdown. Guess I gotta implement ACPI AML?\n");

  return -ENOTIMPL;
}



ksh_def("shutdown", "turn off the machine") {
  sys::shutdown();
  return 0;
}
