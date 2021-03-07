#pragma once
#include <multiboot2.h>
#include <types.h>

namespace kargs {
  void init(uint64_t mbd);

  bool exists(const char *name);
  const char *get(const char *name,
                  const char *def = NULL /* default to NULL if param not found */);
};  // namespace kargs
