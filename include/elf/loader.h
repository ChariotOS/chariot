#pragma once


#include <fs.h>
#include <types.h>
#include <mm.h>
#include "exec_elf.h"

namespace elf {


  bool validate(fs::filedesc &fd);

  bool validate(fs::filedesc &fd, Elf64_Ehdr &);
  int load(const char *, mm::space &vm, ref<fs::filedesc> fd, u64 &entry);

};  // namespace elf
