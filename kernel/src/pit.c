#include <pit.h>
#include <asm.h>
#include <printk.h>


void init_pit(void) {
  // enable the PIT
  outb(0x20, 0x11);  // restart PIC1
  outb(0xA0, 0x11);  // restart PIC2

  outb(0x21, 32);  // PIC1 now starts at 32
  outb(0xA1, 40);  // PIC2 now starts at 40

  outb(0x21, 4);  // setup cascading
  outb(0xA1, 2);

  outb(0x20, 0x01);
  outb(0xA1, 0x01);  // done

  outb(0x43, 0x36);  // tell the PIT which channel we're setting
}

void set_pit_freq(u16 per_sec) {

  // find the divisor
  u64 t = 1193182 / per_sec;

  outb(0x43, 0x36); // select the device
  outb(0x40, t & 0xFF); // send the lower half
  outb(0x40, (t >> 8) & 0xFF); // and the upper half
}
