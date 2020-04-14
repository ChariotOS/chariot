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
#include <vconsole.h>
#include <vga.h>

#include "../drivers/majors.h"
#include "vga_font_raw.h"
#include "xterm_colors.h"

struct [[gnu::packed]] font_file_header {
  char magic[4];
  unsigned char glyph_width;
  unsigned char glyph_height;
  unsigned char type;
  unsigned char is_variable_width;
  unsigned char glyph_spacing;
  unsigned char unused[5];
  char name[64];
};

const struct font_file_header &vga_font = (font_file_header &)*&vga_font_raw;

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

struct vconsole *vga_console = NULL;

static spinlock fblock;
static bool owned = false;

u32 *vga_fba = 0;
struct ck_fb_info info {
  .active = 0, .width = 0, .height = 0,
};

static void set_pixel(uint32_t x, uint32_t y, int color) {
  if (x > info.width || y > info.height) return;
  ((u32 *)p2v(vga_fba))[x + info.width * y] = color;
}

// dummy
static void draw_char(char c, int x, int y, int fg, int bg) {
  auto *rows = const_cast<unsigned *>((const unsigned *)(&vga_font + 1));

  uint32_t *ch = rows + (c * vga_font.glyph_height);

  if (!(c >= ' ' && c <= '~')) {
    for (int r = 0; r < vga_font.glyph_height; r++) {
      for (int c = 0; c < vga_font.glyph_width; c++) {
	set_pixel(x + c, y + r, bg);
      }
    }
    return;
  }

  for (int r = 0; r < vga_font.glyph_height; r++) {
    for (int c = 0; c < vga_font.glyph_width; c++) {
      int b = (ch[r] & (1 << (c))) != 0;

      int color = b ? fg : bg;
      set_pixel(x + c, y + r, color);
    }
  }
}

static void flush_vga_console() {
  vga_console->for_each_cell([](int row, int col, char c, char attr) {
    draw_char(c, col * vga_font.glyph_width, row * vga_font.glyph_height,
	      0xffffff, 0);
  });
}

void vga::putchar(char c) {
  if (vga_console == NULL) return;

  vga_console->feed(c);
  // if (!owned) flush_vga_console();
}

static void set_register(u16 index, u16 data) {
  outw(VBE_DISPI_IOPORT_INDEX, index);
  outw(VBE_DISPI_IOPORT_DATA, data);
}

/*
static u16 get_register(u16 index) {
  outw(VBE_DISPI_IOPORT_INDEX, index);
  return inw(VBE_DISPI_IOPORT_DATA);
}
*/

static void set_info(struct ck_fb_info i) {
  set_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
  set_register(VBE_DISPI_INDEX_XRES, (u16)i.width);
  set_register(VBE_DISPI_INDEX_YRES, (u16)i.height);
  set_register(VBE_DISPI_INDEX_VIRT_WIDTH, (u16)i.width);
  set_register(VBE_DISPI_INDEX_VIRT_HEIGHT, 4096);
	// bits per pixel
	set_register(VBE_DISPI_INDEX_BPP, 32);
	set_register(VBE_DISPI_INDEX_ENABLE,
	 VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
	set_register(VBE_DISPI_INDEX_BANK, 0);

  info.active = i.active;
  info.width = i.width;
  info.height = i.height;
  if (vga_console != NULL && info.active == false) {
    vga_console->resize(info.width / vga_font.glyph_width,
			info.height / vga_font.glyph_height);
  }
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
  i.width = 1024;
  i.height = 768;
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

void vga::early_init(void) {}

static void vga_char_drawer(int x, int y, char c, char attr) {
  if (info.active) return;
  draw_char(c, x * vga_font.glyph_width, y * vga_font.glyph_height,
	    xterm_colors[attr & 0xF], xterm_colors[(attr >> 4) & 0xF]);
}

void vga_mod_init(void) {
  if (vga_fba == NULL) {
    vga_fba = (u32 *)get_framebuffer_address();
  }


  vga_console =
      new vconsole(0, 0, vga_char_drawer);

  auto i = info;
  i.active = false;
  i.width = 1024;
  i.height = 768;
  set_info(i);


  dev::register_driver(generic_driver_info);
  dev::register_name(generic_driver_info, "fb", 0);
}

module_init("vga framebuffer", vga_mod_init);
