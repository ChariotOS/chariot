#include <arch.h>
#include <riscv/arch.h>
#include <printf.h>

unsigned long arch_seconds_since_boot() { return read_csr(time) / CONFIG_RISCV_CLOCKS_PER_SECOND; }
