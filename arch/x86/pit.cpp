#include <asm.h>
#include <idt.h>
#include <lock.h>
#include <pit.h>
#include <printk.h>

#define PIT_A 0x40
#define PIT_B 0x41
#define PIT_C 0x42
#define PIT_CONTROL 0x43

#define PIC_ICW1 0x11
#define PIC_ICW4_MASTER 0x01
#define PIC_ICW4_SLAVE 0x05
#define PIC_ACK_SPECIFIC 0x60

static uint8_t pic_control[2] = {0x20, 0xa0};
static uint8_t pic_data[2] = {0x21, 0xa1};

void set_pit_timeout(u16 ms) {
  if (ms == 0) ms = 1;
  u64 t = 1193182 / (1000 / (ms * 1000));
  outb(0x43, 0x34);             // select the device
  outb(0x40, t & 0xFF);         // send the lower half
  outb(0x40, (t >> 8) & 0xFF);  // and the upper half
}



void set_pit_freq(u16 per_sec) {
  if (per_sec == 0) {
    outb(0x43, 0x30);  // disable the pit's functionality

    outb(0x40, 0);  // send the lower half
    outb(0x40, 0);  // and the upper half
    return;
  }
  // find the divisor
  u64 t = 1193182 / per_sec;

  outb(0x43, 0x36);             // select the device
  outb(0x40, t & 0xFF);         // send the lower half
  outb(0x40, (t >> 8) & 0xFF);  // and the upper half
}

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
    // pic_enable(2);
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

static spinlock pit_sleep_lock;

static spinlock pit_result_lock;

int waiting_10ms_remaining = 0;

void pit_irq_handler(int i, reg_t *, void *) {
  pit_result_lock.unlock();
}

void pit::dumb_sleep(unsigned ms) {
  pit_sleep_lock.lock();
  irq::install(0, pit_irq_handler, "PIT Timer Interrupt");

  pit_result_lock.lock();
  set_pit_timeout(ms);
  pit_result_lock.lock();
  pit_result_lock.unlock();

  irq::uninstall(32);

  pit_sleep_lock.unlock();
}
