#include <riscv/sbi.h>
#include <riscv/paging_flags.h>
#include <riscv/arch.h>

#define rvboot __attribute__((section(".boot.text")))



extern "C" {
extern rv::xsize_t kernel_page_table[4096 / sizeof(rv::xsize_t)];
}
__initdata rv::xsize_t *kernel_page_table_ptr = kernel_page_table;


static inline rvboot void putc(char c) { sbi_call(SBI_CONSOLE_PUTCHAR, c); }
#define nl() putc('\n')


static rvboot char hex_nibble(int value) {
  if (value < 10) return '0' + value;
  return 'A' + (value - 10);
}

static rvboot void print_byte(uint64_t val) {
  putc(hex_nibble((val >> 4) & 0xF));
  putc(hex_nibble((val >> 0) & 0xF));
}

extern "C" rvboot void setup_pagetables(rv::xsize_t *kpt) {
  const rv::xsize_t nents = 4096 / sizeof(rv::xsize_t);
  const rv::xsize_t half = nents / 2;


  for (int i = 0; i < half; i++) {
    rv::xsize_t pte = MAKE_PTE(i * VM_BIGGEST_PAGE, PT_R | PT_W | PT_X | PT_V | PT_A | PT_D);
    kpt[i] = pte;
    kpt[i + half] = pte;
  }
  return;
}
