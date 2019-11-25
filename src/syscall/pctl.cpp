#include <cpu.h>
#include <pctl.h>
#include <process.h>
#include <elf/image.h>
#include <paging.h>


#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))


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
