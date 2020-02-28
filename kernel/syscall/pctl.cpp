#include <cpu.h>
#include <elf/loader.h>
#include <paging.h>
#include <pctl.h>
#include <syscall.h>



static int do_create_thread(struct pctl_create_thread_args *argp) {
  return -1;
}

int sys::pctl(int pid, int request, u64 arg) {
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
