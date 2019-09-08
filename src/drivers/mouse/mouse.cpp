#include <dev/char_dev.h>
#include <dev/driver.h>
#include <errno.h>
#include <mem.h>
#include <module.h>
#include <printk.h>
#include <cpu.h>
#include <vga.h>
#include "../majors.h"
#include <idt.h>

uint8_t mouse_cycle = 0;
char mouse_byte[3];

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
  char t = inb(MOUSE_PORT);
  return t;
}

#define LEFT_CLICK 0x01
#define RIGHT_CLICK 0x02
#define MIDDLE_CLICK 0x04

typedef struct {
  uint32_t magic;
  char x_difference;
  char y_difference;
  u32 buttons;
} mouse_device_packet_t;

int buttons;
int mouse_x, mouse_y;

static void mouse_handler(int i, struct trapframe* tf) {
  cpu::scoped_cli scli;

  u8 status = inb(MOUSE_STATUS);

  while (status & MOUSE_BBIT) {
    i8 mouse_in = inb(MOUSE_PORT);

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
            /* x/y overflow? bad packet! */
            break;
          }
          mouse_device_packet_t packet;
          packet.magic = MOUSE_MAGIC;
          packet.x_difference = mouse_byte[1];
          packet.y_difference = mouse_byte[2];
          packet.buttons = 0;
          if (mouse_byte[0] & 0x01) {
            packet.buttons |= LEFT_CLICK;
          }
          if (mouse_byte[0] & 0x02) {
            packet.buttons |= RIGHT_CLICK;
          }
          if (mouse_byte[0] & 0x04) {
            packet.buttons |= MIDDLE_CLICK;
          }
          mouse_cycle = 0;

          mouse_x = mouse_x + packet.x_difference;
          if (mouse_x < 0) mouse_x = 0;
          if (mouse_x >= vga::width() - 1) mouse_x = vga::width() - 1;
          mouse_y = mouse_y - packet.y_difference;
          if (mouse_y < 0) mouse_y = 0;
          if (mouse_y >= vga::height() - 1) mouse_y = vga::height() - 1;
          buttons = packet.buttons;

          break;
      }
    }

    status = inb(MOUSE_STATUS);
  }
}

void mouse_install() {
  // disable irq in this scope
  cpu::scoped_cli scli;

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
}

class mouse_dev : public dev::char_dev {
 public:
  mouse_dev(ref<dev::driver> dr) : dev::char_dev(dr) {}

  virtual int read(u64 offset, u32 len, void *dst) override {
    return 0;
  }
  virtual int write(u64 offset, u32 len, const void *) override {
    return 0;
  }
  virtual ssize_t size(void) override { return mem_size(); }
};

class mousedev_driver : public dev::driver {
 public:
  mousedev_driver() {
    // register the memory device on minor 0
    m_dev = make_ref<mouse_dev>(ref<mousedev_driver>(this));
    dev::register_name("mouse", MAJOR_MOUSE, 0);
  }

  virtual ~mousedev_driver(){};

  ref<dev::device> open(major_t maj, minor_t min, int &err) { return m_dev; }

  virtual const char *name(void) const { return "mouse"; }

 private:
  ref<mouse_dev> m_dev;
};

static void dev_init(void) {
  dev::register_driver(MAJOR_MOUSE, make_ref<mousedev_driver>());
}

module_init("mouse", dev_init);
