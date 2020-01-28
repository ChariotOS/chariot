#include <process.h>
#include <cpu.h>
#include <mmap_flags.h>
#include <paging.h>
#include <vm.h>


void *sys::mmap(void *addr, size_t length, int prot, int flags, int fd,
                off_t offset) {
  auto proc = cpu::task()->proc;
  if (!proc) return MAP_FAILED;

  // TODO: handle address requests!
  if (addr != NULL) return MAP_FAILED;

  int reg_prot = PTE_U;

  if (prot == PROT_NONE) {
    reg_prot = PTE_U | PTE_W;
  } else {
    if (prot | PROT_READ) reg_prot |= PTE_U;  // not sure for PROT_READ
    if (prot | PROT_WRITE) reg_prot |= PTE_W;
  }
  // if (prot | PROT_EXEC) reg_prot |= PTE_NX; // not sure

  off_t va = proc->mm.add_mapping("mmap", length, reg_prot);

  return (void *)va;
}
int sys::munmap(void *addr, size_t length) { return -1; }

