#include <cpu.h>
#include <elf/loader.h>
#include <pctl.h>
#include <syscall.h>



int sys::pctl(int pid, int request, u64 arg) {
  return -1;
}
