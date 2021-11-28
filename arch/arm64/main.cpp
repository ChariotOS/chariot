#include <stddef.h>
#include <stdint.h>
#include <types.h>
#include <arm64/arch.h>

extern "C" char __end[];


/* First C/C++ function called from start.S */
extern "C" void kernel_main(uint64_t dtb, uint64_t x1, uint64_t x2, uint64_t x3) {
  arm64::platform_init(dtb, x1, x2, x3);

  while (1) {
  }
}
