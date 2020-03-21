#include <asm.h>
#include <console.h>
#include <cpu.h>
#include <dev/driver.h>
#include <errno.h>
#include <fb.h>
#include <mem.h>
#include <module.h>
#include <pci.h>
#include <printk.h>
#include <util.h>
#include <vga.h>

#include "../drivers/majors.h"

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA 0x01CF

#define VBE_DISPI_INDEX_ID 0x0
#define VBE_DISPI_INDEX_XRES 0x1
#define VBE_DISPI_INDEX_YRES 0x2
#define VBE_DISPI_INDEX_BPP 0x3
#define VBE_DISPI_INDEX_ENABLE 0x4
#define VBE_DISPI_INDEX_BANK 0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x7
#define VBE_DISPI_INDEX_X_OFFSET 0x8
#define VBE_DISPI_INDEX_Y_OFFSET 0x9
#define VBE_DISPI_DISABLED 0x00
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_LFB_ENABLED 0x40

#define BXVGA_DEV_IOCTL_SET_Y_OFFSET 1982
#define BXVGA_DEV_IOCTL_SET_RESOLUTION 1985

#define COLUMNS 80
#define LINES 25
#define NPAR 16

static unsigned short *origin = (unsigned short *)p2v(VGA_BASE_ADDR);
static unsigned long pos = 0;
static unsigned long x = 0, y = 0;
static unsigned long bottom = LINES;
static unsigned long lines = LINES, columns = COLUMNS;
static unsigned long state = 0;
// parameter storage
static unsigned long npar, par[NPAR];
static unsigned long ques = 0;
static unsigned char attr = 0x07;

/*
 * this is what the terminal answers to a ESC-Z or csi0c
 * query (= vt100 response).
 */
#define RESPONSE "\033[?1;2c"

static inline void gotoxy(unsigned int new_x, unsigned int new_y) {
  if (new_x >= columns || new_y >= lines) return;
  x = new_x;
  y = new_y;
  pos = y * columns + x;
}

static inline void write(long pos, uint16_t val) {
  if (pos <= 0 || pos >= COLUMNS * LINES) return;
  origin[pos] = val;
}

void scrollup(void) {
  for (int i = 0; i < COLUMNS * (LINES - 1); i++)
    origin[i] = origin[i + COLUMNS];

  // fill the last line with spaces
  for (int i = COLUMNS * (LINES - 1); i < COLUMNS * LINES; i++)
    origin[i] = 0x0720;
}

static void lf(void) {
  if (y + 1 < bottom) {
    y++;
    pos += columns;
    return;
  }
  scrollup();
}

static void cr(void) {
  pos -= x;
  x = 0;
}

static void del(void) {
  if (x) {
    pos--;
    x--;
    write(pos, 0x0720);
  }
}

static inline void set_cursor(void) {
  arch::cli();
  outb(0x3d4, 14);
  outb(0x3d5, 0xff & (pos >> 8));
  outb(0x3d4, 15);
  outb(0x3d5, 0xff & pos);
  arch::sti();
}

static void csi_J(int par) {
  long count = 0;
  long start = 0;
  switch (par) {
    case 0: /* erase from cursor to end of display */
      count = (COLUMNS * LINES - pos);
      start = pos;
      break;
    case 1: /* erase from start to cursor */
      count = pos;
      start = 0;
      break;
    case 2: /* erase whole display */
      count = columns * lines;
      start = 0;
      break;
    default:
      return;
  }

  for (int i = start; i < start + count; i++) write(i, 0x0720);
}

static void csi_K(int par) {
  long count;
  long start;

  switch (par) {
    case 0: /* erase from cursor to end of line */
      if (x >= columns) return;
      count = columns - x;
      start = pos;
      break;
    case 1: /* erase from start of line to cursor */
      start = pos - x;
      count = (x < columns) ? x : columns;
      break;
    case 2: /* erase whole line */
      start = pos - x;
      count = columns;
      break;
    default:
      return;
  }

  for (int i = start; i < start + count; i++) write(i, 0x0720);
}

static int saved_x = 0;
static int saved_y = 0;

static void save_cur(void) {
  saved_x = x;
  saved_y = y;
}

static void restore_cur(void) {
  x = saved_x;
  y = saved_y;
  pos = y * columns + x;
}

void csi_m(void) {
  int i;

  for (i = 0; i <= npar; i++) {
    char p = par[i];

    if (p == 0) {
      attr = 0x07;
    } else if (p >= 30 && p <= 37) {
      attr = (attr & 0xF0) | (((p - 30) & 0xF));
      continue;
    } else if (p >= 40 && p <= 47) {
      attr = (attr & 0xF0) | (((p - 40) & 0xF));
      continue;

    } else if (p >= 90 && p <= 97) {
      attr = (attr & 0xF0) | (((p - 90) & 0xF));
      continue;
    } else {
      switch (par[i]) {
	case 0:
	  attr = 0x07;
	  break;
	case 1:
	  attr = 0x0f;
	  break;
	case 4:
	  attr = 0x0f;
	  break;
	case 7:
	  attr = 0x70;
	  break;
	case 27:
	  attr = 0x07;
	  break;
      }
    }
  }
}

void vga::putchar(char c) {
  switch (state) {
    case 0:
      if (c > 31 && c < 127) {
	if (x >= columns) {
	  x -= columns;
	  pos -= columns;
	  lf();
	}

	write(pos++, (attr << 8) | (c & 0xFF));
	x++;

      } else if (c == 27) {
	state = 1;
      } else if (c == '\n') {
	cr();
	lf();
      } else if (c == '\r') {
	cr();
      } else if (c == CONS_DEL) {
	del();
      } else if (c == 8) {
	if (x) {
	  x--;
	  pos--;
	}
      } else if (c == 9) {
	/*
	c = 8 - (x & 7);
	x += c;
	pos += c << 1;
	if (x > columns) {
	  x -= columns;
	  pos -= columns << 1;
	  lf();
	}
	c = 9;
	*/
      }
      break;
    case 1:
      state = 0;
      if (c == '[') {
	state = 2;
      } else if (c == 'E') {
	gotoxy(0, y + 1);
      } else if (c == 'M') {
	// ri();
      } else if (c == 'D') {
	lf();
      } else if (c == 'Z') {
	// respond(tty);
      } else if (x == '7') {
	save_cur();
      } else if (x == '8') {
	restore_cur();
      }
      break;
    case 2:
      for (npar = 0; npar < NPAR; npar++) par[npar] = 0;
      npar = 0;
      state = 3;
      if ((ques = (c == '?'))) {
	break;
      }
    case 3:
      if (c == ';' && npar < NPAR - 1) {
	npar++;
	break;
      } else if (c >= '0' && c <= '9') {
	par[npar] = 10 * par[npar] + c - '0';
	break;
      } else
	state = 4;
    case 4:
      state = 0;
      switch (c) {
	case 'G':
	case '`':
	  if (par[0]) par[0]--;
	  gotoxy(par[0], y);
	  break;
	case 'A':
	  if (!par[0]) par[0]++;
	  gotoxy(x, y - par[0]);
	  break;
	case 'B':
	case 'e':
	  if (!par[0]) par[0]++;
	  gotoxy(x, y + par[0]);
	  break;
	case 'C':
	case 'a':
	  if (!par[0]) par[0]++;
	  gotoxy(x + par[0], y);
	  break;
	case 'D':
	  if (!par[0]) par[0]++;
	  gotoxy(x - par[0], y);
	  break;
	case 'E':
	  if (!par[0]) par[0]++;
	  gotoxy(0, y + par[0]);
	  break;
	case 'F':
	  if (!par[0]) par[0]++;
	  gotoxy(0, y - par[0]);
	  break;
	case 'd':
	  if (par[0]) par[0]--;
	  gotoxy(x, par[0]);
	  break;
	case 'H':
	case 'f':
	  if (par[0]) par[0]--;
	  if (par[1]) par[1]--;
	  gotoxy(par[1], par[0]);
	  break;
	case 'J':
	  csi_J(par[0]);
	  break;
	case 'K':
	  csi_K(par[0]);
	  break;
	case 'm':
	  csi_m();
	  break;
	  /*
	  case 'L':
		  csi_L(par[0]);
		  break;
	  case 'M':
		  csi_M(par[0]);
		  break;
	  case 'P':
		  csi_P(par[0]);
		  break;
	  case '@':
		  csi_at(par[0]);
		  break;
	  case 'r':
		  if (par[0]) par[0]--;
		  if (!par[1]) par[1]=lines;
		  if (par[0] < par[1] &&
		      par[1] <= lines) {
			  top=par[0];
			  bottom=par[1];
		  }
		  break;
	  case 's':
		  save_cur();
		  break;
	  case 'u':
		  restore_cur();
		  break;
	  */
      }
  }

  set_cursor();
}

static spinlock fblock;
static bool owned = false;

u32 *vga_fba = 0;
struct ck_fb_info info {
  .active = 0, .width = 0, .height = 0,
};

static void set_register(u16 index, u16 data) {
  outw(VBE_DISPI_IOPORT_INDEX, index);
  outw(VBE_DISPI_IOPORT_DATA, data);
}

static u16 get_register(u16 index) {
  outw(VBE_DISPI_IOPORT_INDEX, index);
  return inw(VBE_DISPI_IOPORT_DATA);
}


static void print_vga_state(void) {
	auto enabled = get_register(VBE_DISPI_INDEX_ENABLE);
  printk("enable = %s (%x)\n", (enabled & 0x01) ? "yes" : "no", enabled);
  printk("xres   = %dpx\n", get_register(VBE_DISPI_INDEX_XRES));
  printk("yres   = %dpx\n", get_register(VBE_DISPI_INDEX_YRES));
  printk("vw     = %dpx\n", get_register(VBE_DISPI_INDEX_VIRT_WIDTH));
  printk("vh     = %dpx\n", get_register(VBE_DISPI_INDEX_VIRT_HEIGHT));
}

static void set_info(struct ck_fb_info i) {
  set_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
  set_register(VBE_DISPI_INDEX_XRES, (u16)i.width);
  set_register(VBE_DISPI_INDEX_YRES, (u16)i.height);
  set_register(VBE_DISPI_INDEX_VIRT_WIDTH, (u16)i.width);
  set_register(VBE_DISPI_INDEX_VIRT_HEIGHT, 4096);
  if (i.active) {
  // bits per pixel
    set_register(VBE_DISPI_INDEX_BPP, 32);
    set_register(VBE_DISPI_INDEX_ENABLE,
		 VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    set_register(VBE_DISPI_INDEX_BANK, 0);
	}

  info.active = i.active;
  info.width = i.width;
  info.height = i.height;
}

pci::device *vga_dev = NULL;

static void *get_framebuffer_address(void) {
  void *addr = nullptr;
  pci::walk_devices([&](pci::device *dev) {
    if (dev->is_device(0x1234, 0x1111) || dev->is_device(0x80ee, 0xbeef)) {
      vga_dev = dev;
      addr = (void *)(dev->get_bar(0).raw & 0xfffffff0l);
    }
  });
  return addr;
}

static ssize_t fb_write(fs::file &fd, const char *buf, size_t sz) {
  if (fd) {
    if (vga_fba == nullptr) return -1;

    size_t fbsize = info.width * info.height * sizeof(u32);
    auto off = fd.offset() % fbsize;
    ssize_t space_left = fbsize - off;

    ssize_t to_copy = min(space_left, sz);
    if (to_copy <= 0) return 0;

    cpu::pushcli();
    memcpy((char *)p2v(vga_fba) + off, buf, to_copy);
    cpu::popcli();

    // seek past
    fd.seek(sz, SEEK_CUR);
    return sz;
  }
  return -1;
}

static int fb_ioctl(fs::file &fd, unsigned int cmd, unsigned long arg) {
  scoped_lock l(fblock);
  // pre-cast (dunno if this is dangerous)
  auto *a = (struct ck_fb_info *)arg;
  if (!curproc->mm->validate_struct<struct ck_fb_info>(
	  arg, VALIDATE_READ | VALIDATE_WRITE))
    return -1;

  switch (cmd) {
    case FB_SET_INFO:
      set_info(*a);
      return 0;

      break;

    case FB_GET_INFO:
      *a = info;
      break;

    default:
      return -1;
      break;
  }
  return -1;
}

static int fb_open(fs::file &f) {
  scoped_lock l(fblock);
  // printk("[fb] open: %sallowed\n", owned ? "not " : "");

  if (owned) {
    return -EBUSY;  // disallow
  }
  owned = true;
  return 0;  // allow
}

static void fb_close(fs::file &f) {
  scoped_lock l(fblock);
  // printk("[fb] closed!\n");
  owned = false;

  // disable the framebuffer (drop back to text mode)
  auto i = info;
  i.active = false;
	i.width = 0;
	i.height = 0;
  set_info(i);
}

// can only write to the framebuffer
struct fs::file_operations fb_ops = {
    .write = fb_write,
    .ioctl = fb_ioctl,

    .open = fb_open,
    .close = fb_close,
};

static struct dev::driver_info generic_driver_info {
  .name = "vga framebuffer", .type = DRIVER_CHAR, .major = MAJOR_FB,

  .char_ops = &fb_ops,
};

void vga::early_init(void) { gotoxy(0, 0); }

void vga_mod_init(void) {
  origin = (unsigned short *)p2v(VGA_BASE_ADDR);

  if (vga_fba == NULL) {
    vga_fba = (u32 *)get_framebuffer_address();
  }

  dev::register_driver(generic_driver_info);
  dev::register_name(generic_driver_info, "fb", 0);
}

module_init("vga framebuffer", vga_mod_init);
