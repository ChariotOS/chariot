#include <dev/driver.h>
#include <errno.h>
#include <fifo_buf.h>
#include <idt.h>
#include <mem.h>
#include <module.h>
#include <mouse_packet.h>
#include <printk.h>
#include <sched.h>
#include <smp.h>
#include <vga.h>

#include "../majors.h"

static fifo_buf mouse_buffer;


static bool open = false;
static uint8_t mouse_cycle = 0;
static char mouse_byte[3];

#define PACKETS_IN_PIPE 1024
#define DISCARD_POINT 32

#define MOUSE_IRQ 12

#define MOUSE_PORT 0x60
#define MOUSE_STATUS 0x64
#define MOUSE_ABIT 0x02
#define MOUSE_BBIT 0x01
#define MOUSE_WRITE 0xD4
#define MOUSE_F_BIT 0x20
#define MOUSE_V_BIT 0x08
#define MOUSE_MAGIC 0xFEED1234

void mouse_wait(uint8_t a_type) {
  uint32_t timeout = 100000;
  if (!a_type) {
    while (--timeout) {
      if ((inb(MOUSE_STATUS) & MOUSE_BBIT) == 1) {
        return;
      }
    }
    printk("[MOUSE] mouse timeout\n");
    return;
  } else {
    while (--timeout) {
      if (!((inb(MOUSE_STATUS) & MOUSE_ABIT))) {
        return;
      }
    }
    printk("[MOUSE] mouse timeout\n");
    return;
  }
}

void mouse_write(uint8_t write) {
  mouse_wait(1);
  outb(MOUSE_STATUS, MOUSE_WRITE);
  mouse_wait(1);
  outb(MOUSE_PORT, write);
}

uint8_t mouse_read() {
  mouse_wait(0);
  uint8_t t = inb(MOUSE_PORT);
  return t;
}

static int buttons;
static int mouse_x, mouse_y;

static void mouse_handler(int i, struct task_regs *tf) {
  // reset the cycle
  mouse_cycle = 0;

  // sched::play_tone(440, 25);
  while (1) {
    u8 status = inb(MOUSE_STATUS);

    if ((status & MOUSE_BBIT) == 0) {
      break;
    }

    u8 mouse_in = inb(MOUSE_PORT);

    if (status & MOUSE_F_BIT) {
      switch (mouse_cycle) {
        case 0:
          mouse_byte[0] = mouse_in;
          if (!(mouse_in & MOUSE_V_BIT)) return;
          ++mouse_cycle;
          break;
        case 1:
          mouse_byte[1] = mouse_in;
          ++mouse_cycle;
          break;
        case 2:
          mouse_byte[2] = mouse_in;
          /* We now have a full mouse packet ready to use */
          if (mouse_byte[0] & 0x80 || mouse_byte[0] & 0x40) {
            printk("bad packet!\n");
            /* x/y overflow? bad packet! */
            break;
          }
          struct mouse_packet packet;
          packet.magic = MOUSE_MAGIC;
          packet.dx = mouse_byte[1];
          packet.dy = mouse_byte[2];
          packet.buttons = 0;
          if (mouse_byte[0] & 0x01) {
            packet.buttons |= MOUSE_LEFT_CLICK;
          }
          if (mouse_byte[0] & 0x02) {
            packet.buttons |= MOUSE_RIGHT_CLICK;
          }
          if (mouse_byte[0] & 0x04) {
            packet.buttons |= MOUSE_MIDDLE_CLICK;
          }
          mouse_cycle = 0;

          mouse_x = mouse_x + packet.dx;
          if (mouse_x < 0) mouse_x = 0;
          if (mouse_x >= vga::width() - 1) mouse_x = vga::width() - 1;
          mouse_y = mouse_y - packet.dy;
          if (mouse_y < 0) mouse_y = 0;
          if (mouse_y >= vga::height() - 1) mouse_y = vga::height() - 1;
          buttons = packet.buttons;

          // printk("   %d:%d\n", packet.dx, -packet.dy);
          if (open) mouse_buffer.write(&packet, sizeof(packet));
          // printk("%d bytes in buffer\n", mouse_buffer.size());

          break;
      }
    }
  }

  smp::lapic_eoi();
}


void mouse_install() {
  mouse_wait(1);
  outb(MOUSE_STATUS, 0xA8);
  mouse_wait(1);
  outb(MOUSE_STATUS, 0x20);
  mouse_wait(0);
  u8 status = inb(0x60) | 2;
  mouse_wait(1);
  outb(MOUSE_STATUS, 0x60);
  mouse_wait(1);
  outb(MOUSE_PORT, status);
  mouse_write(0xF6);
  mouse_read();
  mouse_write(0xF4);
  mouse_read();


  interrupt_register(32 + MOUSE_IRQ, mouse_handler);
  smp::ioapicenable(MOUSE_IRQ, 0);
}

static ssize_t mouse_fdread(fs::filedesc &fd, char *buf, size_t sz) {
  if (fd) {
    // if the size of the read request is not a multiple of a packet, fail
    if (sz % sizeof(struct mouse_packet) != 0) return -1;

    auto k = mouse_buffer.read(buf, sz);
    fd.seek(k);
    return k;
  }
  return -1;
}

static int mouse_open(fs::filedesc &fd) {

  // only open if it isn't already opened
  if (open) return -1;
  open = true;
  KINFO("[mouse] open!\n");
  return 0;
}

static void mouse_close(fs::filedesc &fd) {
  open = false;
  KINFO("[mouse] close!\n");
}

struct dev::driver_ops mouse_ops = {
    .read = mouse_fdread,

    .open = mouse_open,
    .close = mouse_close,
};

static void mouse_init(void) {
  mouse_install();

  dev::register_driver("mouse", CHAR_DRIVER, MAJOR_MOUSE, &mouse_ops);
}

module_init("mouse", mouse_init);
