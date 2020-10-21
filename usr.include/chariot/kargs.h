#pragma once
#include <multiboot.h>

namespace kargs {
void init(struct multiboot_info *);

bool exists(const char *name);
const char *get(
    const char *name,
    const char *def = NULL /* default to NULL if param not found */);
};  // namespace kargs

