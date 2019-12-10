#include <cpu.h>
#include <dev/char_dev.h>
#include <dev/driver.h>
#include <errno.h>
#include <fifo_buf.h>
#include <idt.h>
#include <mem.h>
#include <module.h>
#include <mouse_packet.h>
#include <printk.h>
#include <sched.h>
#include <vga.h>
#include "../majors.h"

static fifo_buf mouse_buffer(4096, true);

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
  char t = inb(MOUSE_PORT);
  return t;
}

static int buttons;
static int mouse_x, mouse_y;

static void mouse_handler(int i, struct task_regs *tf) {
  cpu::pushcli();

  // printk("HANDLE...");

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
          mouse_packet_t packet;
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
          mouse_buffer.write(&packet, sizeof(packet));
          // printk("%d bytes in buffer\n", mouse_buffer.size());

          break;
      }
    }
  }

  // printk("OK\n");

  cpu::popcli();
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
}

class mouse_dev : public dev::char_dev {
 public:
  mouse_dev(ref<dev::driver> dr) : dev::char_dev(dr) {}

  virtual int read(u64 offset, u32 len, void *dst) override {
    return mouse_buffer.read((u8 *)dst, len);
  }
  virtual int write(u64 offset, u32 len, const void *) override { return 0; }
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


  virtual ssize_t read(minor_t, fs::filedesc &fd, void *buf, size_t sz) {
    return -1;
  };


 private:
  ref<mouse_dev> m_dev;
};

static void dev_init(void) {
  // mouse_install();
  // dev::register_driver(MAJOR_MOUSE, make_ref<mousedev_driver>());
}

module_init("mouse", dev_init);
