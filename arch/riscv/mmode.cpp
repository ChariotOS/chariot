#include <arch.h>
#include <console.h>
#include <cpu.h>
#include <phys.h>
#include <riscv/arch.h>
#include <riscv/dis.h>
#include <riscv/paging.h>
#include <riscv/plic.h>
#include <riscv/uart.h>
#include <util.h>

extern "C" void _start(void);
extern "C" void timervec();
extern "C" void machine_trapvec();
extern "C" void supervisor_trapvec();
static volatile bool phys_ready = false;
void timerinit();

extern void main();

// static __initdata void *main_ptr = (void *)main;
// static __initdata void *timervec_ptr = (void *)timervec;


// extern "C" rv::xsize_t kernel_page_table[];
