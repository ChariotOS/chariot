#pragma once

#include <arch.h>

#include <ck/vec.h>

namespace debug {

  // prints a register with a nice color scheme that indicates
  // the different types of bytes in the register. Useful
  // for debugging buffer overruns and whatnot
  void print_register(const char *name, reg_t reg);


  ck::vec<off_t> generate_backtrace(off_t bp);
  // void print_register(reg_t reg) { debug::print_register(NULL, reg); }
};  // namespace debug
