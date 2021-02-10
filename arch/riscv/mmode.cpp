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

__initdata rv::xsize_t kernel_page_table[4096 / sizeof(rv::xsize_t)] __attribute__((aligned(4096)));
extern "C" char _bss_start[];
extern "C" char _bss_end[];
extern "C" char _stack_start[];




// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L /* Not virtual, as this is used in machine modew */
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8 * (hartid))
#define CLINT_MTIME (CLINT + 0xBFF8)  // cycles since boot.



extern "C" void __init setup_mhart_state(rv::hart_state *hs, rv::xsize_t dtb) {
  int id = read_csr(mhartid);

  hs->hartid = id;
  hs->dtb = (dtb::fdt_header *)dtb;


  // ask the CLINT for a timer interrupt.
  int interval = TICK_INTERVAL;
  *(uint64_t *)CLINT_MTIMECMP(id) = *(uint64_t *)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  hs->tca = CLINT_MTIMECMP(id);
  hs->interval = interval;
}


