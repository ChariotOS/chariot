#pragma once
#include <vec.h>
#include <string.h>

// the kshell related function calls are for debugging the kernel from userspace.
// (or somewhere else, if you want I guess). They arent meant to return anything,
// but just output to the debug console (ideally)
// Imagine it like an actually command line interface, but as an IPC interface to the kernel.
// They all require root (uid=0) to run :)
namespace kshell {
  using handler = unsigned long (*)(vec<string> &, void *data, int dlen);
  void add(string command, handler h);
  unsigned long call(string command, vec<string> args, void *data, size_t dlen);
};  // namespace kshell
