#include <asm.h>
#include <console.h>
#include <cpu.h>
#include <dev/driver.h>
#include <errno.h>
#include <fb.h>
#include <mem.h>
#include <module.h>
#include <pci.h>
#include <phys.h>
#include <printf.h>
#include <util.h>
#include <vconsole.h>
#include <vga.h>
#include <dev/video.h>
#include <multiboot2.h>
#include "arch.h"
#include <mm.h>

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

#include <device_majors.h>
#include "font.ckf.h"

struct [[gnu::packed]] chariot_kernel_font {
  uint8_t width;
  uint8_t height;
  uint16_t data[]; /* width * height uint16 values */
};

const auto &vga_font = (chariot_kernel_font &)*&build_font_ckf;



#define VCONSOLE_HEIGHT CONFIG_FRAMEBUFFER_HEIGHT
#define VCONSOLE_WIDTH CONFIG_FRAMEBUFFER_WIDTH
#define FONT_SCALE (1)
#define EDGE_MARGIN 0
#define TOTAL_MARGIN (EDGE_MARGIN * 2)
#define FONT_WIDTH (7)  // hardcoded :/
#define FONT_HEIGHT (13)

#define CHAR_LINE_MARGIN (0)
#define LINE_HEIGHT (FONT_HEIGHT + CHAR_LINE_MARGIN)

#define VC_COLS (((VCONSOLE_WIDTH - TOTAL_MARGIN) / FONT_WIDTH) / FONT_SCALE)
#define VC_ROWS (((VCONSOLE_HEIGHT - TOTAL_MARGIN) / LINE_HEIGHT) / FONT_SCALE)

static void vga_char_scribe(int col, int row, struct vc_cell *, int flags);

// manually create
struct vc_cell static_cells[VC_COLS * VC_ROWS];
struct vcons vga_console {
  .state = 0, .cols = VC_COLS, .rows = VC_ROWS, .pos = 0, .x = 0, .y = 0, .saved_x = 0, .saved_y = 0, .scribe = vga_char_scribe, .npar = 0,
  .ques = 0, .attr = 0x07, .buf = (struct vc_cell *)&static_cells,
};

static bool cons_enabled = false;
static spinlock fblock;

static u32 *vga_fba = 0;
struct ck_fb_info info {
  .active = 0, .width = 0, .height = 0,
};

static inline void set_pixel(uint32_t x, uint32_t y, int color) {
  if (cons_enabled) {
    if (x > info.width || y > info.height) return;
    ((u32 *)p2v(vga_fba))[x + info.width * y] = color;
  }
}

static void flush_vga_console() {
  if (cons_enabled) {
    for (int y = 0; y < VC_ROWS; y++) {
      for (int x = 0; x < VC_COLS; x++) {
        int cursor = vga_console.x == x && vga_console.y == y;
        vga_char_scribe(x, y, vc_get_cell(&vga_console, x, y), cursor);
      }
    }
  }
}


// returns the userspace address
void *vga::get_fba(void) { return (void *)p2v(vga_fba); }

void vga::putchar(char c) {
  if (cons_enabled) {
    /* TODO(nick): move the virtual console stuff into the GVI interface */
    vc_feed(&vga_console, c);
  }
}

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
  set_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
  set_register(VBE_DISPI_INDEX_BANK, 0);

  info.active = i.active;
  info.width = i.width;
  info.height = i.height;
}


void vga::configure(struct ck_fb_info &i) {
  if (i.active == false) {
    i.width = VCONSOLE_WIDTH;
    i.height = VCONSOLE_HEIGHT;
  }
  cons_enabled = !i.active;
  set_info(i);
  flush_vga_console();
}


static ck::ref<hw::PCIDevice> vga_dev = nullptr;

// static pci::device *vga_dev = NULL;

static void *get_framebuffer_address(void) {
  if (vga_dev) return (void *)(vga_dev->get_bar(0).raw & 0xfffffff0l);

  return NULL;
}




static ssize_t fb_write(fs::File &fd, const char *buf, size_t sz) {
  if (!fd) return -1;
  if (vga_fba == nullptr) return -1;

  size_t fbsize = info.width * info.height * sizeof(u32);
  auto off = fd.offset() % fbsize;
  ssize_t space_left = fbsize - off;

  ssize_t to_copy = min(space_left, sz);
  if (to_copy <= 0) return 0;

  memcpy((char *)p2v(vga_fba) + off, buf, to_copy);

  // seek past
  fd.seek(sz, SEEK_CUR);
  return sz;
}

static int fb_ioctl(fs::File &fd, unsigned int cmd, unsigned long arg) {
  scoped_lock l(fblock);

  switch (cmd) {
    case FB_SET_INFO:
      if (vga_dev == nullptr) return -1;
      if (!curproc->mm->validate_struct<struct ck_fb_info>(arg, VALIDATE_READ | VALIDATE_WRITE)) {
        return -1;
      }
      set_info(*(struct ck_fb_info *)arg);
      return 0;

      break;

    case FB_GET_INFO:
      if (!curproc->mm->validate_struct<struct ck_fb_info>(arg, VALIDATE_READ | VALIDATE_WRITE)) {
        return -1;
      }
      *(struct ck_fb_info *)arg = info;
      break;

    default:
      return -1;
      break;
  }
  return -1;
}



static int fg_colors[] = {
    0x676767,
    0xff6d67,
    0x59f68d,
    0xf3f89d,
    0xc9a8fa,
    0xff92d0,
    0x99ecfd,
    0xfeffff,
};

static int bg_colors[] = {
    0x000000,
    0xff6d67,
    0x59f68d,
    0xf3f89d,
    0xc9a8fa,
    0xff92d0,
    0x99ecfd,
    0xc7c7c7,
};

static void vga_char_scribe(int x, int y, struct vc_cell *cell, int flags) {
  if (!cons_enabled) return;
  char c = cell->c;
  char attr = cell->attr;

  auto ch = (uint16_t *)vga_font.data + (c * FONT_HEIGHT);
  x = EDGE_MARGIN + x * FONT_WIDTH;
  y = EDGE_MARGIN + y * LINE_HEIGHT;
  int fg = fg_colors[attr & 0xF];
  int bg = bg_colors[(attr >> 4) & 0xF];

  // flip around the colors for the block cursor
  if (flags & VC_SCRIBE_CURSOR) {
    int tmp = fg;
    fg = bg;
    bg = tmp;
  }

  for (int r = 0; r < LINE_HEIGHT; r++) {
    for (int c = 0; c <= FONT_WIDTH; c++) {
      int b = 0;
      if (r < FONT_HEIGHT) b = (ch[r] & (1 << (FONT_WIDTH - c))) != 0;

      int color = b ? fg : bg;
      for (int ox = 0; ox < FONT_SCALE; ox++)
        for (int oy = 0; oy < FONT_SCALE; oy++)
          set_pixel(x * FONT_SCALE + c * FONT_SCALE + ox, y * FONT_SCALE + r * FONT_SCALE + oy, color);
    }
  }
}




void vga::early_init(uint64_t mbd) {
  mb2::find<struct multiboot_tag_framebuffer_common>(mbd, MULTIBOOT_TAG_TYPE_FRAMEBUFFER, [](auto *i) {
    vga_fba = (uint32_t *)i->framebuffer_addr;
#ifdef CONFIG_FRAMEBUFFER_AUTODETECT

    info.width = i->framebuffer_width;
    info.height = i->framebuffer_height;
#else
        info.height = CONFIG_FRAMEBUFFER_HEIGHT;
        info.width = CONFIG_FRAMEBUFFER_WIDTH;
#endif

    info.active = false;

    KINFO("width: %d, height: %d, addr: %p\n", info.width, info.height, i->framebuffer_addr);
    vga::configure(info);
  });


  if (vga_fba == NULL) vga_fba = (u32 *)get_framebuffer_address();
}



class VGADevice : public dev::VideoDevice {
 public:
  using dev::VideoDevice::VideoDevice;
  virtual ~VGADevice(void) {}

  int get_mode(gvi_video_mode &mode) override {
    mode.width = info.width;
    mode.height = info.height;
    mode.caps = 0;
    return 0;
  }

  void init(void) override {
    vga_dev = dev();
    register_instance();
  }

  uint32_t *get_framebuffer(void) override {
    if (vga_fba == NULL) {
      KINFO("Looking for VGA Framebuffer\n");
      vga_fba = (uint32_t *)get_framebuffer_address();
    }
    return vga_fba;
  }

  int open(fs::File &file) override {
    if (!cons_enabled) return -EBUSY;
    cons_enabled = false;
    return 0;
  }
  void close(fs::File &file) override { cons_enabled = true; }
};



static dev::ProbeResult vga_probe(ck::ref<hw::Device> dev) {
  if (vga_dev.is_null()) {
    if (auto pci = dev->cast<hw::PCIDevice>()) {
      if (pci->is_device(0x1234, 0x1111) || pci->is_device(0x80ee, 0xbeef)) {
        return dev::ProbeResult::Attach;
      }
    }
  }
  return dev::ProbeResult::Ignore;
};


driver_init("x86-vga", VGADevice, vga_probe);
