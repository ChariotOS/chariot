#include <cpu.h>
#include <mmap_flags.h>
#include <paging.h>
#include <process.h>
#include <vm.h>

void *sys::mmap(void *addr, long length, int prot, int flags, int fd,
                long offset) {
  auto proc = cpu::proc();
  if (!proc) return MAP_FAILED;

  // TODO: handle address requests!
  if (addr != NULL) return MAP_FAILED;

  int reg_prot = 0;

  if (prot != PROT_NONE) {
    if (prot & PROT_READ) reg_prot |= VPROT_READ;  // not sure for PROT_READ
    if (prot & PROT_WRITE) reg_prot |= VPROT_WRITE;
    if (prot & PROT_EXEC) reg_prot |= VPROT_EXEC;
  }

  off_t va = proc->addr_space->add_mapping("", length, reg_prot);

  return (void *)va;
}

int sys::munmap(void *addr, size_t length) { return -1; }

int sys::mrename(void *addr, char *name) {
  auto proc = cpu::proc();
  if (!proc->addr_space->validate_string(name)) return -1;

  string sname = name;
  for (auto &c : sname) {
    if (c < ' ') c = ' ';
    if (c > '~') c = ' ';
  }

  auto region = proc->addr_space->lookup((u64)addr & ~0xFFF);

  if (region == NULL) return -ENOENT;
  region->name = move(sname);
  return 0;
}

