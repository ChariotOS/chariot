#include <asm.h>
#include <cpu.h>
#include <elf/image.h>
#include <errno.h>
#include <fs/vfs.h>
#include <map.h>
#include <mem.h>
#include <mmap_flags.h>
#include <paging.h>
#include <pctl.h>
#include <phys.h>
#include <process.h>
#include <syscalls.h>
#include <util.h>
#include <vga.h>

extern "C" void trapret(void);

void fd_flags::clear() {
  fd = nullptr;
  flags = 0;
}

/**
 * ===============================================
 * end thread functions
 * begin system calls
 */

// static long sys_invalid(u64, u64, u64, u64, u64, u64) { return 0; }

#define SYSSYM(name) sys_##name

void sys::restart() {
  // TODO: NEED TO RESTART
}

void sys::exit() {
  // TODO: NEED TO EXIT
}

int sys::open(const char *path, int flags, int mode) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}

int sys::close(int fd) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}

off_t sys::lseek(int fd, off_t offset, int whence) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}

ssize_t sys::read(int fd, void *dst, size_t len) {
  panic("IMPL %s\n", __PRETTY_FUNCTION__);
  return -ENOTIMPL;
}

ssize_t sys::write(int fd, void *data, size_t len) {
  auto proc = cpu::proc();

  if (!proc->mm.validate_pointer(data, len, VALIDATE_READ)) {
    return -1;
  }

  for (int i = 0; i < len; i++) {
    printk("%c", ((char *)data)[i]);
  }
  return 0;
}

int sys::yield(void) {
  sched::yield();
  return 0;
}

// XXX: REMOVE
int sys::set_pixel(int x, int y, int color) {
  vga::set_pixel(x, y, color);
  return 0;
}

pid_t sys::getpid(void) { return cpu::task()->pid; }

pid_t sys::gettid(void) { return cpu::task()->tid; }

pid_t do_spawn(void) {
  assert(cpu::in_thread());
  auto proc = cpu::proc();

  int err = 0;
  auto p = task_process::spawn(proc->pid, err);

  if (err != 0) {
    // TODO: make sure the process is deleted
    return -1;
  }

  proc->nursery.push(p->pid);

  return p->pid;
}

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

static bool validate_nt_string_array(struct task_process *proc, char **a) {
  // validate the argv
  if (!proc->mm.validate_pointer(a, sizeof(char **), VALIDATE_READ)) {
    return false;
  }
  // loop over and check all the entry strings
  for (int i = 0; true; i++) {
    if (!proc->mm.validate_pointer(a + i, sizeof(char *), VALIDATE_READ))
      return false;
    if (a[i] == nullptr) break;
    if (!proc->mm.validate_string(a[i])) return false;
  }
  return true;
}

static int do_cmd(pid_t pid, struct pctl_cmd_args *args) {
  auto proc = cpu::proc().get();

  if (proc->pid != 0) {
    // validate the args structure itself
    if (!proc->mm.validate_pointer(args, sizeof(*args), VALIDATE_READ)) {
      return -1;
    }

    // validate the absolute path
    if (!proc->mm.validate_string(args->path)) {
      return -1;
    }

    // validate argv and env
    if (!proc->mm.validate_pointer(args->argv, sizeof(char *) * args->argc,
                                   VALIDATE_READ))
      return -1;
    if (args->envv != NULL &&
        !proc->mm.validate_pointer(args->envv, sizeof(char *) * args->envc,
                                   VALIDATE_READ)) {
      return -1;
    }
  }

  char *abs_path = args->path;
  char **argv = args->argv;
  char **envp = args->envv;

  // validate the pid first
  bool valid_pid = false;

  int nursery_index = 0;
  // TODO: lock nursery
  for (int i = 0; i < proc->nursery.size(); i++) {
    if (pid == proc->nursery[i]) {
      valid_pid = true;
      nursery_index = i;
      break;
    }
  }

  if (!valid_pid) return -1;

  auto newproc = task_process::lookup(pid);

  // TODO: validate that the current user has access to the file
  auto file = vfs::open(abs_path, 0);

  if (!file) {
    printk("file not found '%s'\n", abs_path);
    return -1;  // file wasn't found TODO: ERRNO CODE
  }

  u64 entry_address = 0;

  // TODO: avoid reading the entire elf file :)
  auto sz = file->size();
  auto buf = kmalloc(sz);
  auto fd = fs::filedesc(file, FDIR_READ);
  int nread = fd.read(buf, sz);
  elf::image im((u8 *)buf, nread);

  if (!im.valid()) {
    kfree(buf);
    return -1;
  }

  for (int i = 0; i < im.section_count(); i++) {
    auto sec = im.get_section(i);

    // probably not the best thing to do, but I'm not really sure
    if (sec.address() == 0) continue;

    auto name = string::format("%s(%s)", abs_path, sec.name());

    // KWARN("%s at %p\n", name.get(), sec.address());
    // if the segment needs to be loaded
    if (sec.type() == PT_LOAD) {
      // map it
      newproc->mm.map_file(move(name), file, sec.address(), sec.offset(),
                           sec.size(), PTE_W | PTE_U | PTE_P);
    } else if (sec.type() == 8 /* TODO: investigate BSS type*/) {
      int pages = round_up(sec.size(), 4096) >> 12;
      auto reg = make_ref<vm::memory_backing>(pages);
      newproc->mm.add_mapping(move(name), sec.address(), pages * PGSIZE,
                              move(reg), PTE_W | PTE_U);
    }
  }
  entry_address = im.header().e_entry;

  kfree(buf);

  u64 stack = 0;

  // map the user stack
  auto ustack_size = 8 * PGSIZE;
  stack = newproc->mm.add_mapping("[stack]", ustack_size, PTE_W | PTE_U);
  stack += ustack_size - 32;

  for (int i = 0; argv[i] != NULL; i++) {
    newproc->args.push(argv[i]);
  }

  if (envp != NULL) {
    for (int i = 0; envp[i] != NULL; i++) {
      newproc->env.push(envp[i]);
    }
  }

  int tid = newproc->create_task(nullptr, 0, nullptr, PS_EMBRYO);

  cpu::pushcli();
  auto t = task::lookup(tid);

  // t->tf->rdi = (u64)u_argc;
  // t->tf->rsi = (u64)u_argv;
  // tf->rdx = u_envp

  t->tf->esp = stack;
  t->tf->eip = entry_address;
  t->state = PS_RUNNABLE;

  cpu::popcli();

  proc->nursery.remove(nursery_index);

  return 0;
}

int sys::pctl(int pid, int request, u64 arg) {
  // printk("pctl(%d, %d, %p);\n", pid, request, arg);
  switch (request) {
    case PCTL_SPAWN:
      return do_spawn();

    case PCTL_CMD:
      return do_cmd(pid, (struct pctl_cmd_args *)arg);

    // request not handled!
    default:
      return -1;
  }
  return -1;
}

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

// WARNING: HACK
struct syscall {
  const char *name;
  int num;
  void *handler;
};

map<int, struct syscall> syscall_table;
// vec<struct syscall> syscall_table;

void set_syscall(const char *name, int num, void *handler) {
  // KINFO("%s -> %d\n", name, num);
  syscall_table[num] = {.name = name, .num = num, .handler = handler};
}

void syscall_init(void) {
#undef __SYSCALL
#define __SYSCALL(num, name) set_syscall(#name, num, (void *)sys::name);
#include <syscalls.inc>
}

static u64 do_syscall(long num, u64 a, u64 b, u64 c, u64 d, u64 e, u64 f) {
  if (!syscall_table.contains(num) || syscall_table[num].handler == nullptr) {
    KWARN("unknown syscall in pid %d. syscall(%d) @ rip=%p\n", cpu::proc()->pid,
          num, cpu::task()->tf->eip);
    return -1;
  }

  auto *func = (u64(*)(u64, u64, u64, u64, u64, u64))syscall_table[num].handler;

  return func(a, b, c, d, e, f);
}

void syscall_handle(int i, struct task_regs *tf) {
  // int x = 0;
  tf->rax =
      do_syscall(tf->rax, tf->rdi, tf->rsi, tf->rdx, tf->r10, tf->r8, tf->r9);
  return;
}

extern "C" long __syscall(size_t, size_t, size_t, size_t, size_t, size_t,
                          size_t);

long ksyscall(long n, ...) {
  va_list ap;
  size_t a, b, c, d, e, f;
  va_start(ap, n);
  a = va_arg(ap, size_t);
  b = va_arg(ap, size_t);
  c = va_arg(ap, size_t);
  d = va_arg(ap, size_t);
  e = va_arg(ap, size_t);
  f = va_arg(ap, size_t);
  va_end(ap);

  return do_syscall(n, a, b, c, d, e, f);
}
