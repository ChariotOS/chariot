#pragma once


#include <fs.h>
#include <types.h>
#include <vm.h>
#include "exec_elf.h"

namespace elf {


  bool validate(fs::filedesc &fd);

  bool validate(fs::filedesc &fd, Elf64_Ehdr &);
  int load(vm::addr_space &vm, fs::filedesc &fd, u64 &entry);

};  // namespace elf
