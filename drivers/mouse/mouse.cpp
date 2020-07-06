#include <arch.h>
#include <cpu.h>
#include <dev/driver.h>
#include <errno.h>
#include <fifo_buf.h>
#include <mem.h>
#include <module.h>
#include <mouse_packet.h>
#include <printk.h>
#include <sched.h>
#include <vmware_backdoor.h>
#include "../majors.h"

static fifo_buf mouse_buffer;

#define MOUSE_DEFAULT 0
#define MOUSE_SCROLLWHEEL 1
#define MOUSE_BUTTONS 2

static bool open = false;
static uint8_t mouse_cycle = 0;
static char mouse_byte[5];
static char mouse_mode = MOUSE_DEFAULT;

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

void mouse_wait(uint8_t a_type) {
  uint32_t timeout = 10000;
  if (!a_type) {
    while (--timeout) {
      if ((inb(MOUSE_STATUS) & MOUSE_BBIT) == 1) {
        return;
      }
    }
    // printk("[MOUSE] mouse timeout\n");
    return;
  } else {
    while (--timeout) {
      if (!((inb(MOUSE_STATUS) & MOUSE_ABIT))) {
        return;
      }
    }
    // printk("[MOUSE] mouse timeout\n");
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


#define CMD_ABSPOINTER_DATA 39
#define CMD_ABSPOINTER_STATUS 40
#define CMD_ABSPOINTER_COMMAND 41

static void mouse_handler(int i, reg_t *) {
#if 0
  if (vmware::vmmouse_is_absolute()) {
    inb(0x60);
    struct vmware::command cmd;
    /* Read the mouse status */
    cmd.bx = 0;
    cmd.cmd = CMD_ABSPOINTER_STATUS;
    vmware::send(cmd);

    /* Mouse status is in EAX */
    if (cmd.ax == 0xFFFF0000) {
      /* An error has occured, let's turn the device off and back on */
      // mouse_off();
      // mouse_absolute();
      return;
    }

    /* The status command returns a size we need to read, should be at least 4.
     */
    if ((cmd.ax & 0xFFFF) < 4) {
      return;
    }

    /* Read 4 bytes of mouse data */
    cmd.bx = 4;
    cmd.cmd = CMD_ABSPOINTER_DATA;
    vmware::send(cmd);

    /* Mouse data is now stored in AX, BX, CX, and DX */
    int flags = (cmd.ax & 0xFFFF0000) >> 16; /* Not important */
    int buttons =
        (cmd.ax & 0xFFFF); /* 0x10 = Right, 0x20 = Left, 0x08 = Middle */
    int x = (cmd.bx);      /* Both X and Y are scaled from 0 to 0xFFFF */
    int y =
        (cmd.cx); /* You should map these somewhere to the actual resolution. */
    int z = (char)(cmd.dx); /* Z is a single signed byte indicating scroll
                               direction. */

    (void)flags;
    (void)buttons;
    printk("x: %d, y: %d, z: %d\n", x, y, z);
    irq::eoi(i);
    return;
  }
#endif

  uint8_t status = inb(MOUSE_STATUS);
	cpu::pushcli();

  while ((status & MOUSE_BBIT) && (status & MOUSE_F_BIT)) {
    bool finalize = false;
    char mouse_in = inb(MOUSE_PORT);
    switch (mouse_cycle) {
      case 0:
        mouse_byte[0] = mouse_in;
        if (!(mouse_in & MOUSE_V_BIT)) break;
        ++mouse_cycle;
        break;
      case 1:
        mouse_byte[1] = mouse_in;
        ++mouse_cycle;
        break;
      case 2:
        mouse_byte[2] = mouse_in;
        if (mouse_mode == MOUSE_SCROLLWHEEL || mouse_mode == MOUSE_BUTTONS) {
          ++mouse_cycle;
          break;
        }
        finalize = true;
        break;
      case 3:
        mouse_byte[3] = mouse_in;
        finalize = true;
        break;
    }

    if (finalize) {
      mouse_cycle = 0;
      /* We now have a full mouse packet ready to use */
      struct mouse_packet packet;
      memset(&packet, 0, sizeof(packet));
      packet.magic = MOUSE_MAGIC;
      int x = mouse_byte[1];
      int y = mouse_byte[2];
      if (x && mouse_byte[0] & (1 << 4)) {
        /* Sign bit */
        x = x - 0x100;
      }
      if (y && mouse_byte[0] & (1 << 5)) {
        /* Sign bit */
        y = y - 0x100;
      }
      if (mouse_byte[0] & (1 << 6) || mouse_byte[0] & (1 << 7)) {
        /* Overflow */
        x = 0;
        y = 0;
      }
      packet.dx = x;
      packet.dy = -y;  // the mouse gives us a negative value for going up
      packet.buttons = 0;
#if 0
			for (int i = 0; i < 3; i++) {
				printk("0x%02x ", mouse_byte[i] & 0xFF);
			}
			printk("\n");
#endif
      if (mouse_byte[0] & 0x01) {
        packet.buttons |= MOUSE_LEFT_CLICK;
      }
      if (mouse_byte[0] & 0x02) {
        packet.buttons |= MOUSE_RIGHT_CLICK;
      }
      if (mouse_byte[0] & 0x04) {
        packet.buttons |= MOUSE_MIDDLE_CLICK;
      }

      if (mouse_mode == MOUSE_SCROLLWHEEL && mouse_byte[3]) {
        if ((char)mouse_byte[3] > 0) {
          packet.buttons |= MOUSE_SCROLL_DOWN;
        } else if ((char)mouse_byte[3] < 0) {
          packet.buttons |= MOUSE_SCROLL_UP;
        }
      }

      if (open) {
        mouse_buffer.write(&packet, sizeof(packet), false /* dont block */);
      }
    }
    break;
  }
	cpu::popcli();

  irq::eoi(i);
}

void mouse_install() {
  int result = 0;
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

  // try to enable the scroll wheel
  mouse_write(0xF2);
  mouse_read();
  result = mouse_read();

  mouse_write(0xF3);
  mouse_read();
  mouse_write(200);
  mouse_read();

  mouse_write(0xF3);
  mouse_read();
  mouse_write(100);
  mouse_read();

  mouse_write(0xF3);
  mouse_read();
  mouse_write(80);
  mouse_read();
  mouse_write(0xF2);
  mouse_read();



  result = mouse_read();
  if (result == 3) {
    mouse_mode = MOUSE_SCROLLWHEEL;
  }


  /* mouse scancode set */
  mouse_wait(1);
  outb(MOUSE_PORT, 0xF0);
  mouse_wait(1);
  outb(MOUSE_PORT, 0x02);
  mouse_wait(1);
  mouse_read();



  mouse_write(0xF3);
  mouse_read();
  mouse_write(255);
  mouse_read();


  uint8_t tmp = inb(0x61);
  outb(0x61, tmp | 0x80);
  outb(0x61, tmp & 0x7F);
  inb(MOUSE_PORT);

  while ((inb(0x64) & 1)) {
    inb(0x60);
  }

  irq::install(32 + MOUSE_IRQ, mouse_handler, "PS2 Mouse");
}

static ssize_t mouse_read(fs::file &fd, char *buf, size_t sz) {
  if (fd) {
    if (sz % sizeof(struct mouse_packet) != 0) {
      return -EINVAL;
    }

    // this is a nonblocking api
    return mouse_buffer.read(buf, sz, false);
  }
  return -1;
}

static int mouse_open(fs::file &fd) {
  // only open if it isn't already opened
  if (open) return -1;
  open = true;
  // KINFO("[mouse] open!\n");
  return 0;
}

static void mouse_close(fs::file &fd) {
  open = false;
  // KINFO("[mouse] close!\n");
}


static int mouse_poll(fs::file &fd, int events) {
  return mouse_buffer.poll() & events;
}


struct fs::file_operations mouse_ops = {
    .read = mouse_read,

    .open = mouse_open,
    .close = mouse_close,

    .poll = mouse_poll,
};


static struct dev::driver_info mouse_driver_info {
  .name = "mouse", .type = DRIVER_CHAR, .major = MAJOR_MOUSE,

  .char_ops = &mouse_ops,
};

static void mouse_init(void) {
  mouse_install();


  dev::register_driver(mouse_driver_info);
  dev::register_name(mouse_driver_info, "mouse", 0);
}

module_init("mouse", mouse_init);
