#include <cpu.h>
#include <process.h>

void sys::exit_thread(int code) {
  // if we are the main thread, exit the group instead.
  if (curthd->pid == curthd->tid) {
    sys::exit_proc(code);
    return;
  }

  // ...
  printk("exit_thread(%d)\n", code);
}

void sys::exit_proc(int code) {
  // ...
  printk("exit_proc(%d)\n", code);
}


