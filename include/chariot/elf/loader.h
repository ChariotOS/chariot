#pragma once


#include <fs.h>
#include <types.h>
#include <mm.h>
#include <sched.h>
#include "exec_elf.h"
#include <func.h>

namespace elf {


  bool validate(fs::file &fd);

  bool validate(fs::file &fd, Elf64_Ehdr &);
  int load(const char *, struct process &p, mm::space &mm, ck::ref<fs::file> fd, off_t &entry);

  // iterate for each symbol in an elf file. The callback returns if we should continue
  int each_symbol(fs::file &fd, ck::func<bool(const char *sym, off_t)>);

};  // namespace elf
