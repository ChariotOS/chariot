#include <cpu.h>
#include <elf/loader.h>
#include <syscall.h>
#include <util.h>


// kernel/proc.cpp
extern mm::space *alloc_user_vm(void);


int sys::execve(const char *path, const char **uargv, const char **uenvp) {
  // TODO: this isn't super smart imo, we are relying on not getting an irq
  auto *tf = curthd->trap_frame;

  if (path == NULL) return -EINVAL;
  if (uargv == NULL) return -EINVAL;

  /* Validate the arguments (top level) */
  if (!curproc->mm->validate_string(path)) return -EINVAL;
  if (!curproc->mm->validate_null_terminated(uargv)) return -EINVAL;
  if (!curproc->mm->validate_null_terminated(uenvp)) return -EINVAL;

  ck::vec<ck::string> argv;
  ck::vec<ck::string> envp;

  for (int i = 0; uargv[i] != NULL; i++) {
    /* Validate the string :^) */
    if (!curproc->mm->validate_string(uargv[i])) return -EINVAL;
    argv.push(uargv[i]);
  }

  if (uenvp != NULL) {
    for (int i = 0; uenvp[i] != NULL; i++) {
      /* Validate the string :^) */
      if (!curproc->mm->validate_string(uenvp[i])) return -EINVAL;
      envp.push(uenvp[i]);
    }
  }

  // try to load the binary
  fs::inode *exe = NULL;

  // TODO: open permissions on the binary
  if (vfs::namei(path, 0, 0, curproc->cwd, exe) != 0) {
    return -ENOENT;
  }



  {
    ck::vec<int> to_close;
    scoped_lock l(curproc->file_lock);
    for (auto [fd, file] : curproc->open_files) {
      if (fd >= 3) {
        to_close.push(fd);
      }
    }

    for (int fd : to_close) {
      curproc->open_files.remove(fd);
    }
  }

  off_t entry = 0;
  auto fd = ck::make_ref<fs::file>(exe, FDIR_READ);
  mm::space *new_addr_space = nullptr;

  new_addr_space = alloc_user_vm();

  int loaded = elf::load(path, *curproc, *new_addr_space, fd, entry);
  if (loaded < 0) {
    delete new_addr_space;
    return loaded;
  }

  // printk(KERN_DEBUG "pid %d exec '%s'\n", curproc->pid, path);


  // allocate a 1mb stack
  // TODO: this size is arbitrary.
  auto stack_size = 1024 * 1024;
  off_t stack = new_addr_space->mmap(
      "[stack]", 0, stack_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, nullptr, 0);

  // kill the other threads
  for (auto tid : curproc->threads) {
    auto *thd = thread::lookup(tid);
    if (thd != NULL && thd != curthd) {
      // take the runlock on the thread so nobody else runs it
      thd->locks.run.lock();
      thread::teardown(thd);
    }
  }

  curproc->name = path;
  curproc->args = argv;
  curproc->env = envp;
  new_addr_space->switch_to();

  delete curproc->mm;
  curproc->mm = new_addr_space;

  arch_reg(REG_SP, tf) = stack + stack_size - 64;
  arch_reg(REG_PC, tf) = (unsigned long)entry;

  cpu::switch_vm(curthd);

  curthd->setup_tls();
  curthd->setup_stack(tf);



  /* TODO: on riscv, we overwrite the a0 value with the return value from the systemcall. Instead,
   * we ought to do something smarter, but for now, success means returning the success value from
   * the sys::exec syscall on riscv
   */
#ifdef CONFIG_RISCV
  return ((rv::regs *)tf)->a0;
#endif

  return 0;
}
