#include <riscv/sbi.h>
#include <riscv/paging_flags.h>

#define rvboot __attribute__((section(".boot.text")))


static inline rvboot void putc(char c) {
	sbi_call(SBI_CONSOLE_PUTCHAR, c);
}
#define nl() putc('\n')


static rvboot char hex_nibble(int value) {
	if (value < 10) return '0' + value;
	return 'A' + (value - 10);
}

static rvboot void print_byte(uint64_t val) {
	putc(hex_nibble((val >> 4) & 0xF));
	putc(hex_nibble((val >> 0) & 0xF));
}

rvboot void setup_pagetables(uint64_t *kpt) {
  const uint64_t nents = 4096 / sizeof(uint64_t);
	const uint64_t half = nents / 2;


  for (int i = 0; i < half; i++) {
		uint64_t pte = MAKE_PTE(i * VM_BIGGEST_PAGE, PT_R | PT_W | PT_X | PT_V);
		kpt[i] = pte;
		kpt[i + half] = pte;
  }
  return;
}
