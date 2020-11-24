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
#include <printk.h>
#include <util.h>
#include <vconsole.h>
#include <vga.h>

#include <multiboot2.h>

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

#define EDGE_MARGIN 0
#define TOTAL_MARGIN (EDGE_MARGIN * 2)
#define FONT_WIDTH (7)  // hardcoded :/
#define FONT_HEIGHT (13)

#define CHAR_LINE_MARGIN (0)
#define LINE_HEIGHT (FONT_HEIGHT + CHAR_LINE_MARGIN)

#define VC_COLS ((VCONSOLE_WIDTH - TOTAL_MARGIN) / FONT_WIDTH)
#define VC_ROWS ((VCONSOLE_HEIGHT - TOTAL_MARGIN) / LINE_HEIGHT)

static void vga_char_scribe(int col, int row, struct vc_cell *, int flags);

// manually create
struct vc_cell static_cells[VC_COLS * VC_ROWS];
struct vcons vga_console {
  .state = 0, .cols = VC_COLS, .rows = VC_ROWS, .pos = 0, .x = 0, .y = 0, .saved_x = 0, .saved_y = 0,
  .scribe = vga_char_scribe, .npar = 0, .ques = 0, .attr = 0x07, .buf = (struct vc_cell *)&static_cells,
};

static bool cons_enabled = false;
static spinlock fblock;

u32 *vga_fba = 0;
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




static pci::device *vga_dev = NULL;

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

static int fb_ioctl(fs::file &fd, unsigned int cmd, unsigned long arg) {
  scoped_lock l(fblock);

  switch (cmd) {
    case FB_SET_INFO:
      if (vga_dev == NULL) return -1;
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

static int fb_open(fs::file &f) {
  if (!cons_enabled) return -EBUSY;
  cons_enabled = false;
  return 0;  // allow
}


/*
static void reset_fb(
    void) {  // disable the framebuffer (drop back to text mode)
  auto i = info;
  i.active = false;
  i.width = VCONSOLE_WIDTH;
  i.height = VCONSOLE_HEIGHT;
        cons_enabled = true;
  vga::configure(i);
}
*/

static void fb_close(fs::file &f) {
  cons_enabled = true;
  // reset_fb();
}




struct vga_vmobject final : public mm::vmobject {
  vga_vmobject(size_t npages) : vmobject(npages) {}

  virtual ~vga_vmobject(void){};

  // get a shared page (page #n in the mapping)
  virtual ref<mm::page> get_shared(off_t n) override {
    auto p = mm::page::create((unsigned long)vga_fba + (n * PGSIZE));

    p->nocache = true;
    p->writethrough = true;

    return p;
  }
};




static ref<mm::vmobject> vga_mmap(fs::file &f, size_t npages, int prot, int flags, off_t off) {
  // XXX: this is invalid, should be asserted before here :^)
  if (off != 0) {
    printk(KERN_WARN "vga: attempt to mmap at invalid offset (%d != 0)\n", off);
    return nullptr;
  }

  if (npages > NPAGES(64 * MB)) {
    printk(KERN_WARN "vga: attempt to mmap too many pages (%d pixels)\n", (npages * 4096) / sizeof(uint32_t));
    return nullptr;
  }


  if (flags & MAP_PRIVATE) {
    printk(KERN_WARN "vga: attempt to mmap with MAP_PRIVATE doesn't make sense :^)\n");
    return nullptr;
  }

  return make_ref<vga_vmobject>(npages);
}



// can only write to the framebuffer
struct fs::file_operations fb_ops = {
    .write = fb_write,
    .ioctl = fb_ioctl,

    .open = fb_open,
    .close = fb_close,
    .mmap = vga_mmap,
};

static struct dev::driver_info generic_driver_info {
  .name = "vga framebuffer", .type = DRIVER_CHAR, .major = MAJOR_FB,

  .char_ops = &fb_ops,
};




static int fg_colors[] = {
    0x676767, 0xff6d67, 0x59f68d, 0xf3f89d, 0xc9a8fa, 0xff92d0, 0x99ecfd, 0xfeffff,
};

static int bg_colors[] = {
    0x000000, 0xff6d67, 0x59f68d, 0xf3f89d, 0xc9a8fa, 0xff92d0, 0x99ecfd, 0xc7c7c7,
};

static void vga_char_scribe(int x, int y, struct vc_cell *cell, int flags) {
  if (!cons_enabled) return;
  char c = cell->c;
  char attr = cell->attr;
  if (info.active) return;

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
      set_pixel(x + c, y + r, color);
    }
  }
}




void vga::early_init(uint64_t mbd) {
  /*

  */

  mb2::find<struct multiboot_tag_framebuffer_common>(mbd, MULTIBOOT_TAG_TYPE_FRAMEBUFFER, [](auto *i) {
    vga_fba = (uint32_t *)i->framebuffer_addr;
    info.active = 0;
    info.width = i->framebuffer_width;
    info.height = i->framebuffer_height;
    info.active = false;
    vga::configure(info);
  });


  if (vga_fba == NULL) vga_fba = (u32 *)get_framebuffer_address();
}


void vga_mod_init(void) {
  if (vga_fba != NULL) {
    printk(KERN_INFO "Standard VGA Framebuffer found!\n");
    dev::register_driver(generic_driver_info);
    dev::register_name(generic_driver_info, "fb", 0);
  }
}

module_init("vga framebuffer", vga_mod_init);
