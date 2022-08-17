#include <stddef.h>
#include <stdint.h>
#include <types.h>
#include <arm64/arch.h>
#include <devicetree.h>
#include <util.h>
#include <phys.h>

extern "C" char __end[];


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define ARM64_READ_SYSREG(reg)                              \
  ({                                                        \
    uint64_t _val;                                          \
    __asm__ volatile("mrs %0," TOSTRING(reg) : "=r"(_val)); \
    _val;                                                   \
  })

#define ARM64_WRITE_SYSREG(reg, val)                          \
  ({                                                          \
    __asm__ volatile("msr " TOSTRING(reg) ", %0" ::"r"(val)); \
    ISB;                                                      \
  })

#define PRINT_SYSREG(name) printf("%-20s = 0x%08lx\n", #name, ARM64_READ_SYSREG(name))

typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];
extern "C" char _kernel_end[];

static off_t dtb_ram_start = 0;
static size_t dtb_ram_size = 0;

/* First C/C++ function called from start.S */
extern "C" void kernel_main(uint64_t dtb_raw, uint64_t x1, uint64_t x2, uint64_t x3) {
  arm64::platform_init(dtb_raw, x1, x2, x3);

  printf("Hello from ARM64!\n");
	PRINT_SYSREG(CURRENTEL);
	PRINT_SYSREG(midr_el1);
	PRINT_SYSREG(esr_el1);
	PRINT_SYSREG(far_el1);
	PRINT_SYSREG(cpacr_el1);
	PRINT_SYSREG(ID_AA64MMFR0_EL1);
	PRINT_SYSREG(TTBR0_EL1);
	PRINT_SYSREG(SCTLR_EL1);

	while(1) {}


  auto dtb = (dtb::fdt_header *)dtb_raw;
  hexdump((void *)dtb, 512, true);
  // We can assume we have at least 1MB of ram beyond the kernel itself. Let's
  // use it as boot scratch memory
  off_t boot_free_start = (off_t)v2p(_kernel_end);
  off_t boot_free_end = boot_free_start + 1 * MB;
  phys::free_range((void *)boot_free_start, (void *)boot_free_end);



  dtb::parse(dtb);

  dtb::walk_devices([](dtb::node *node) -> bool {
    printf("node: %s\n", node->name);
    return true;
    if (!strcmp(node->name, "memory")) {
      /* We found the ram (there might be more, but idk for now) */
      dtb_ram_size = node->reg.length;
      dtb_ram_start = node->reg.address;
      return false;
    }
    return true;
  });


  while (1) {
  }
}
