#include <cpu.h>
#include <elf/image.h>
#include <paging.h>
#include <pctl.h>
#include <process.h>

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

  auto fd = fs::filedesc(file, FDIR_READ);

  {
    static const char elf_header[] = {0x7f, 0x45, 0x4c, 0x46};
    // LOG_TIME;

    Elf64_Ehdr hdr;

    fd.seek(0, SEEK_SET);
    int header_read = fd.read(&hdr, sizeof(hdr));

    // verify the header
    bool invalid = false;

    if (!invalid && header_read != sizeof(hdr)) invalid = true;

    // check the elf header identifier
    if (!invalid) {
      for (int i = 0; i < 4; i++) {
        if (hdr.e_ident[i] != elf_header[i]) {
          invalid = true;
          break;
        }
      }
    }

    if (invalid) {
      return -1;
    }

    // the binary is valid, so lets read the headers!
    entry_address = hdr.e_entry;

    Elf64_Shdr *sec_hdrs;

    sec_hdrs = new Elf64_Shdr[hdr.e_shnum];

    fd.seek(hdr.e_shoff, SEEK_SET);
    auto sec_expected = hdr.e_shnum * sizeof(*sec_hdrs);
    auto sec_read = fd.read(sec_hdrs, sec_expected);
    if (sec_read != sec_expected) {
      delete[] sec_hdrs;
      return -1;
    }

    delete[] sec_hdrs;

    if (true) {
      Elf64_Phdr *prog_hdrs;
      // read program headers
      prog_hdrs = new Elf64_Phdr[hdr.e_phnum];
      fd.seek(hdr.e_phoff, SEEK_SET);
      auto hdrs_size = hdr.e_phnum * hdr.e_phentsize;
      auto hdrs_read = fd.read(prog_hdrs, hdrs_size);
      if (hdrs_read != hdrs_size) {
        delete[] prog_hdrs;
        return -1;
      }

      int err = 0;

      for (int i = 0; i < hdr.e_phnum; i++) {
        auto &sec = prog_hdrs[i];

        switch (sec.p_type) {
          case PT_LOAD:
            newproc->mm.map_file("name_me", file, sec.p_vaddr, sec.p_offset,
                                 sec.p_filesz, PTE_W | PTE_U | PTE_P);
            break;
          default:
            // printk("unhandled program header %d\n", sec.p_type);
            break;
        }
      }
      delete[] prog_hdrs;

      if (err != 0) {

      }
    }

    /*
    for (int i = 0; i < hdr.e_shnum; i++) {
      auto &sec = sec_hdrs[i];

      printk("sect header %d, 0x%x  ", i, sec.sh_type);

      printk("%p-%p %d\n", sec.sh_addr, sec.sh_addr + sec.sh_size, sec.sh_size);

      switch (sec.sh_type) {
        // load program data in
        case SHT_PROGBITS:
          // load in program data
          break;

        // bss data
        case SHT_NOBITS: {
          int pages = round_up(sec.sh_size, 4096) >> 12;
          auto reg = make_ref<vm::memory_backing>(pages);
          newproc->mm.add_mapping(".bss", sec.sh_addr, pages * PGSIZE,
                                  move(reg), PTE_W | PTE_U);
        }

        break;

        default:
          // dunno...
          break;
      }
    }
    */
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

  auto t = task::lookup(tid);

  // t->tf->rdi = (u64)u_argc;
  // t->tf->rsi = (u64)u_argv;
  // tf->rdx = u_envp

  t->tf->esp = stack;
  t->tf->eip = entry_address;
  t->state = PS_RUNNABLE;

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
