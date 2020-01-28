#include <asm.h>
#include <pit.h>
#include <printk.h>

#define PIC_ICW1 0x11
#define PIC_ICW4_MASTER 0x01
#define PIC_ICW4_SLAVE 0x05
#define PIC_ACK_SPECIFIC 0x60

static uint8_t pic_control[2] = {0x20, 0xa0};
static uint8_t pic_data[2] = {0x21, 0xa1};

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

  outb(0x43, 0x36);             // select the device
  outb(0x40, t & 0xFF);         // send the lower half
  outb(0x40, (t >> 8) & 0xFF);  // and the upper half
}

void pic_enable(uint8_t irq) {
  uint8_t mask;
  if (irq < 8) {
    mask = inb(pic_data[0]);
    mask = mask & ~(1 << irq);
    outb(pic_data[0], mask);
  } else {
    irq -= 8;
    mask = inb(pic_data[1]);
    mask = mask & ~(1 << irq);
    outb(pic_data[1], mask);
    pic_enable(2);
  }
}

void pic_disable(uint8_t irq) {
  uint8_t mask;
  if (irq < 8) {
    mask = inb(pic_data[0]);
    mask = mask | (1 << irq);
    outb(pic_data[0], mask);
  } else {
    irq -= 8;
    mask = inb(pic_data[1]);
    mask = mask | (1 << irq);
    outb(pic_data[1], mask);
  }
}

void pic_ack(uint8_t irq) {
  if (irq >= 8) {
    outb(pic_control[1], PIC_ACK_SPECIFIC + (irq - 8));
    outb(pic_control[0], PIC_ACK_SPECIFIC + (2));
  } else {
    outb(pic_control[0], PIC_ACK_SPECIFIC + irq);
  }
}
