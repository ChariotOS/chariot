#pragma once

#include <types.h>
#include "image.h"

namespace elf {
class loader {

  elf::image image;

 public:
  // a loader takes the binary data image of the elf and the size of said data
  loader(u8*, int size);
};
};  // namespace elf
