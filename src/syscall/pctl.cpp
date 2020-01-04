#include <cpu.h>
#include <elf/loader.h>
#include <paging.h>
#include <pctl.h>
#include <process.h>

pid_t do_spawn(void) {
  assert(cpu::in_thread());
  auto proc = cpu::proc();

  int err = 0;
  auto p = task_process::spawn(proc->pid, err);

  if (err != 0) {
    // TODO: make sure the process is deleted
    return -1;
  }

  // inherit stdio files
  // TODO: is there a better way of doing this?
  p->open_files[0] = proc->open_files[0];
  p->open_files[1] = proc->open_files[1];
  p->open_files[2] = proc->open_files[2];

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

  auto fd = fs::filedesc(file, FDIR_READ);

  /*
  // validate the elf binary
  if (!elf::validate(fd)) {
    printk("here\n");
    return -1;
  }
  */

  // printk("========================================================\n");
  int loaded = elf::load(newproc->mm, fd, entry_address);
  // printk("========================================================\n");

  if (loaded != 0) {
    return -1;
  }

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

  auto t = task::lookup(tid).get();

  // t->tf->rdi = (u64)u_argc;
  // t->tf->rsi = (u64)u_argv;
  // tf->rdx = u_envp

  t->tf->esp = stack;
  t->tf->eip = entry_address;
  t->state = PS_RUNNABLE;

  proc->nursery.remove(nursery_index);

  return 0;
}

static int do_create_thread(struct pctl_create_thread_args *argp) {
  assert(cpu::in_thread());
  auto proc = cpu::proc();

  /* validate the argument passed */
  if (!proc->mm.validate_pointer(argp, sizeof(argp), VPROT_READ)) {
    return -1;
  }

  auto &args = *argp;

  int tid = proc->create_task(nullptr, 0, nullptr, PS_EMBRYO);
  auto t = task::lookup(tid).get();

  if (t == NULL) {
    panic("failed to spawn thread\n");
  }


  // set up initial context
  t->tf->esp = (u64)args.stack + args.stack_size;
  t->tf->eip = (u64)args.fn;
  t->tf->rdi = (u64)args.arg;
  t->state = PS_RUNNABLE; /* mark task as runnable */

  args.tid = tid;

  return 0;
}

int sys::pctl(int pid, int request, u64 arg) {
  // printk("pctl(%d, %d, %p);\n", pid, request, arg);
  switch (request) {
    case PCTL_SPAWN:
      return do_spawn();

    case PCTL_CMD:
      return do_cmd(pid, (struct pctl_cmd_args *)arg);

    case PCTL_CREATE_THREAD:
      return do_create_thread((struct pctl_create_thread_args *)arg);

    // request not handled!
    default:
      return -1;
  }
  return -1;
}
