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

#include "../drivers/majors.h"
#include "font.ckf.h"

struct [[gnu::packed]] chariot_kernel_font {
  uint8_t width;
  uint8_t height;
  uint16_t data[]; /* width * height uint16 values */
};

const auto &vga_font = (chariot_kernel_font &)*&build_font_ckf;

#define EDGE_MARGIN 5
#define TOTAL_MARGIN (EDGE_MARGIN * 2)
#define CHAR_LINE_MARGIN 4
#define FONT_WIDTH (7)	// hardcoded :/
#define FONT_HEIGHT (13)
#define LINE_HEIGHT (FONT_HEIGHT + CHAR_LINE_MARGIN)

#define VCONSOLE_WIDTH 640
#define VCONSOLE_HEIGHT 480

#define VC_COLS ((VCONSOLE_WIDTH - TOTAL_MARGIN) / FONT_WIDTH)
#define VC_ROWS ((VCONSOLE_HEIGHT - TOTAL_MARGIN) / LINE_HEIGHT)

static void vga_char_scribe(int col, int row, struct vc_cell *);

// manually create
struct vc_cell static_cells[VC_COLS * VC_ROWS];
struct vcons vga_console {
  .state = 0, .cols = VC_COLS, .rows = VC_ROWS, .pos = 0, .x = 0, .y = 0,
  .saved_x = 0, .saved_y = 0, .scribe = vga_char_scribe, .npar = 0, .ques = 0,
  .attr = 0x07,	 // common default
      .buf = (struct vc_cell *)&static_cells,
};

static bool cons_enabled = false;

static spinlock fblock;
static bool owned = false;

u32 *vga_fba = 0;
struct ck_fb_info info {
  .active = 0, .width = 0, .height = 0,
};

static inline void set_pixel(uint32_t x, uint32_t y, int color) {
  if (x > info.width || y > info.height) return;
  ((u32 *)p2v(vga_fba))[x + info.width * y] = color;
}

// dummy
static void draw_char(char c, int x, int y, int fg, int bg) {
  auto ch = (uint16_t*)vga_font.data + (c * FONT_HEIGHT);

  if (!(c >= ' ' && c <= '~')) {
    for (int r = 0; r < LINE_HEIGHT; r++) {
      for (int c = 0; c < FONT_WIDTH; c++) set_pixel(x + c, y + r, bg);
    }
    return;
  }

  for (int r = 0; r < LINE_HEIGHT; r++) {
    for (int c = 0; c < FONT_WIDTH; c++) {
      int b = 0;
      if (r < FONT_HEIGHT) b = (ch[r] & (1 << (c))) != 0;

      int color = b ? fg : bg;
      set_pixel(x + FONT_WIDTH - c, y + r, color);
    }
  }
}

static void flush_vga_console() {
  for (int y = 0; y < VC_ROWS; y++) {
    for (int x = 0; x < VC_COLS; x++) {
      vga_char_scribe(x, y, vc_get_cell(&vga_console, x, y));
    }
  }
}

void vga::putchar(char c) { vc_feed(&vga_console, c); }

static void set_register(u16 index, u16 data) {
  outw(VBE_DISPI_IOPORT_INDEX, index);
  outw(VBE_DISPI_IOPORT_DATA, data);
}

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

static void reset_fb(void) {
  // disable the framebuffer (drop back to text mode)
  auto i = info;
  i.active = false;
  i.width = VCONSOLE_WIDTH;
  i.height = VCONSOLE_HEIGHT;
  set_info(i);

  flush_vga_console();
}

static void fb_close(fs::file &f) {
  scoped_lock l(fblock);
  // printk("[fb] closed!\n");
  owned = false;
  reset_fb();
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

void vga::early_init(void) {
  if (vga_fba == NULL) {
    vga_fba = (u32 *)get_framebuffer_address();
  }

  reset_fb();
  cons_enabled = true;
}

static int fg_colors[] = {
    0x676767, 0xff6d67, 0x59f68d, 0xf3f89d,
    0xc9a8fa, 0xff92d0, 0x99ecfd, 0xfeffff,
};

static int bg_colors[] = {
    0x000000, 0xff6d67, 0x59f68d, 0xf3f89d,
    0xc9a8fa, 0xff92d0, 0x99ecfd, 0xc7c7c7,
};

static void vga_char_scribe(int x, int y, struct vc_cell *cell) {
  char c = cell->c;
  char attr = cell->attr;
  if (info.active) return;
  draw_char(c, EDGE_MARGIN + x * FONT_WIDTH, EDGE_MARGIN + y * LINE_HEIGHT,
	    fg_colors[attr & 0xF], bg_colors[(attr >> 4) & 0xF]);
}

void vga_mod_init(void) {
  dev::register_driver(generic_driver_info);
  dev::register_name(generic_driver_info, "fb", 0);
}

module_init("vga framebuffer", vga_mod_init);
