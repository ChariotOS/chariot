#include <arch.h>
#include <console.h>
#include <cpu.h>
#include <phys.h>
#include <printk.h>
#include <riscv/arch.h>
#include <riscv/dis.h>
#include <riscv/plic.h>
#include <riscv/uart.h>
#include <util.h>




typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];
extern "C" void _start(void);

extern "C" char _bss_start[];
extern "C" char _bss_end[];
extern "C" void machine_trapvec();
extern "C" void supervisor_trapvec();


static inline void hang_forever(void) {
  while (1) {
    arch_halt();
  }
}

static volatile bool phys_ready = false;

void timerinit();
extern "C" void timervec();

extern void main();

/*
 * kstart - C++ entrypoint in machine mode.
 * The point of this function is to get the hart into supervisor mode quickly.
 */
extern "C" void kstart(void) {
  int id = read_csr(mhartid);
  if (id == 0) {
    rv::uart_init();
    printk(KERN_DEBUG "Zeroing BSS: [%llx-%llx]\n", _bss_start, _bss_end);
    for (char *c = _bss_start; c != _bss_end; c++) *c = 0;
  }

  struct rv::scratch sc;
  sc.hartid = id;

  /* set M "previous privilege mode" to supervisor in mstatus, for mret */
  auto x = read_csr(mstatus);
  x &= ~MSTATUS_MPP_MASK;  // mstatus.MPP = 0
  x |= MSTATUS_MPP_S;      // mstatus.MPP = S
  write_csr(mstatus, x);

  /* mret to a certain routine based on hartid (0 is the main hart) */
  write_csr(mepc, main);

  /* Disable paging in supervisor mode */
  write_csr(satp, 0);

  // delegate all interrupts and exceptions to supervisor mode.
  write_csr(medeleg, 0xffff);
  write_csr(mideleg, 0xffff);
  write_csr(sie, read_csr(sie) | SIE_SEIE | SIE_STIE | SIE_SSIE);



  // ask the CLINT for a timer interrupt.
  int interval = TICK_INTERVAL;  // cycles; about 1/10th second in qemu.
  *(uint64_t *)CLINT_MTIMECMP(id) = *(uint64_t *)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..2] : space for timervec to save registers.
  // scratch[3] : address of CLINT MTIMECMP register.
  // scratch[4] : desired interval (in cycles) between timer interrupts.
  sc.tca = CLINT_MTIMECMP(id);
  sc.interval = interval;
  write_csr(mscratch, &sc);

  // set the machine-mode trap handler.
  write_csr(mtvec, timervec);

  // enable machine-mode interrupts.
  write_csr(mstatus, read_csr(mstatus) | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  write_csr(mie, read_csr(mie) | MIE_MTIE);

  rv::set_tp((rv::xsize_t)&sc);

  /* Switch to supervisor mode and jump to main() */
  asm volatile("mret");
}




// set up to receive timer interrupts in machine mode,
// which arrive at timervec in lowlevel.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void timerinit() {
  // each CPU has a separate source of timer interrupts.
  int id = read_csr(mhartid);
}

