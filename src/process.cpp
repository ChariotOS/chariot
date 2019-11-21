#include <asm.h>
#include <cpu.h>
#include <elf/image.h>
#include <errno.h>
#include <fs/vfs.h>
#include <mem.h>
#include <phys.h>
#include <process.h>
#include <syscalls.h>
#include <util.h>
#include <vga.h>
#include <map.h>
#include <paging.h>

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
    // printk("%p: !! invalid !!\n", data);
    return -1;
  }

  printk("%p: ", data);
  hexdump(data, len);
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

pid_t sys::spawn(void) {
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

int sys::impersonate(pid_t) { return -ENOTIMPL; }

int sys::cmdpidve(pid_t pid, const char *abs_path, const char *argv[],
                  const char *envp[]) {

  auto proc = cpu::proc();

  bool valid_pid = false;

  // TODO: lock nursery
  for (int i = 0; i < proc->nursery.size(); i++) {
    if (pid == proc->nursery[i]) {
      valid_pid = true;
      proc->nursery.remove(i);
      break;
    }
  }

  auto newproc = task_process::lookup(pid);


  // TODO: validate that the current user has access to the file
  auto file = vfs::open(abs_path, 0);

  if (!file) {
    return -1; // file wasn't found TODO: ERRNO CODE
  }
  newproc->mm.map_file(abs_path, file, 0x1000, 0, file->size(), PTE_W | PTE_U);

  auto ustack_size = 4 * PGSIZE;
  // map the user stack
  auto stack = newproc->mm.add_mapping("stack", ustack_size, PTE_W | PTE_U);

  int tid = newproc->create_task(nullptr, 0, nullptr, PS_EMBRYO);


  cpu::pushcli();
  auto t = task::lookup(tid);
  t->tf->esp = stack + ustack_size - 512;
  t->tf->eip = 0x1000;
  t->state = PS_RUNNABLE;

  cpu::popcli();



  return 0;
}




// WARNING: HACK
struct syscall {
  const char *name;
  int num;
  void *handler;
};

map<int, struct syscall> syscall_table;
// vec<struct syscall> syscall_table;

void set_syscall(const char *name, int num, void *handler) {

  syscall_table[num] = {.name = name, .num = num, .handler = handler};
}

void syscall_init(void) {
#undef __SYSCALL
#define __SYSCALL(num, name) set_syscall(#name, num, (void *)sys::name);
#include <syscalls.inc>
}

static long do_syscall(long num, u64 a, u64 b, u64 c, u64 d, u64 e) {

  if (num <= 0 || num >= syscall_table.size() || syscall_table[num].handler == nullptr) {
    return -1;
  }

  /*
  KINFO("syscall(%s, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx)\n", syscall_table[num].name, a, b, c, d, e);
  */

  auto *func = (long (*)(u64, u64, u64, u64, u64))syscall_table[num].handler;

  return func(a, b, c, d, e);
}

void syscall_handle(int i, struct task_regs *tf) {
  // int x = 0;
  // printk("rax=%p krsp~%p\n", tf->rax, &x);
  tf->rax = do_syscall(tf->rax, tf->rdi, tf->rsi, tf->rdx, tf->r10, tf->r8);
  return;
}

extern "C" long __syscall(size_t, size_t, size_t, size_t, size_t, size_t,
                          size_t);

long ksyscall(long n, ...) {
  va_list ap;
  size_t a, b, c, d, e;
  va_start(ap, n);
  a = va_arg(ap, size_t);
  b = va_arg(ap, size_t);
  c = va_arg(ap, size_t);
  d = va_arg(ap, size_t);
  e = va_arg(ap, size_t);
  va_end(ap);

  return do_syscall(n, a, b, c, d, e);
}
