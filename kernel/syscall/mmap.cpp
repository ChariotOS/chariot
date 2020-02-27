#include <cpu.h>
#include <mmap_flags.h>
#include <paging.h>
#include <process.h>
#include <mm.h>

void *sys::mmap(void *addr, long length, int prot, int flags, int fd,
                long offset) {
  auto proc = cpu::proc();
  if (!proc) return MAP_FAILED;

  // TODO: handle address requests!
  if (addr != NULL) return MAP_FAILED;

  ref<fs::filedesc> f = nullptr;

  if ((flags & MAP_ANON) == 0) {
    f = proc->get_fd(fd);
  }

  off_t va = proc->mm->mmap((off_t)addr, length, prot, flags, f, offset);

  return (void *)va;
}

int sys::munmap(void *addr, size_t length) {
  auto proc = cpu::proc();
  if (!proc) return -1;

  return proc->mm->unmap((off_t)addr, length);
}

int sys::mrename(void *addr, char *name) {
  auto proc = cpu::proc();

  if (!proc->mm->validate_string(name)) return -1;



  string sname = name;
  for (auto &c : sname) {
    if (c < ' ') c = ' ';
    if (c > '~') c = ' ';
  }

  auto region = proc->mm->lookup((u64)addr & ~0xFFF);

  if (region == NULL) return -ENOENT;

  region->name = move(sname);
  return 0;
}

