#include <arch.h>
#include <platform.h>
#include <platform/bcm28xx.h>
#include <platform/mailbox.h>


#ifdef CONFIG_BCM2837
/* initial memory mappings. parsed by start.S */
struct mmu_region mmu_mappings[] = {
    /* 1GB of sdram space */
    {.start = 0,
     // .virt = KERNEL_BASE,
     .size = MEMORY_APERTURE_SIZE,
     .flags = 0,
     .name = "memory"},

    /* peripherals */
    {.start = BCM_PERIPH_BASE_PHYS,
     // .virt = BCM_PERIPH_BASE_VIRT,
     .size = BCM_PERIPH_SIZE,
     .flags = MMU_MAPPING_FLAG_DEVICE,
     .name = "bcm peripherals"},

    /* null entry to terminate the list */
    {0}};
#endif


unsigned long bcm28xx_mmio_base = 0;

void rpi_detect(void) {
  uint64_t reg;
  uint64_t *mmio_base;
  char *board;

  /* read the system register */
  asm volatile("mrs %0, midr_el1" : "=r"(reg));

  /* get the PartNum, detect board and MMIO base address */
  switch ((reg >> 4) & 0xFFF) {
    case 0xB76:
      bcm28xx_mmio_base = 0x20000000;
      break;
    case 0xC07:
      bcm28xx_mmio_base = 0x3F000000;
      break;
    case 0xD03:
      bcm28xx_mmio_base = 0x3F000000;
      break;
    case 0xD08:
      bcm28xx_mmio_base = 0xFE000000;
      break;
    default:
      bcm28xx_mmio_base = 0x20000000;
      break;
  }
}

void arm64_platform_init(uint64_t dtb, uint64_t x1, uint64_t x2, uint64_t x3) {
  /* Detect the rpi  */
  serial_install();


  serial_send(0, 'A');

  while (1) {
  }

  serial_string(0, (char *)"Hello, world\n");
  while (1) {
  }
}


/* TODO: */
#if 0
void _start()
{
}
#endif
