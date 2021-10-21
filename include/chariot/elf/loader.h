#pragma once


#include <fs.h>
#include <types.h>
#include <mm.h>
#include <sched.h>
#include "exec_elf.h"
#include <ck/func.h>

#include <process.h>

namespace elf {


  bool validate(fs::File &fd);

  bool validate(fs::File &fd, Elf64_Ehdr &);
  int load(const char *, Process &p, mm::AddressSpace &mm, ck::ref<fs::File> fd, off_t &entry);

  // iterate for each symbol in an elf File. The callback returns if we should continue
  int each_symbol(fs::File &fd, ck::func<bool(const char *sym, off_t)>);

};  // namespace elf
