#pragma once


#include <fs.h>
#include <types.h>
#include <mm.h>
#include "exec_elf.h"

namespace elf {


  bool validate(fs::file &fd);

  bool validate(fs::file &fd, Elf64_Ehdr &);
  int load(const char *, mm::space &vm, ref<fs::file> fd, u64 &entry);

};  // namespace elf
