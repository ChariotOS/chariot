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

#include "../drivers/majors.h"
#include "font.ckf.h"

struct [[gnu::packed]] chariot_kernel_font {
  uint8_t width;
  uint8_t height;
  uint16_t data[]; /* width * height uint16 values */
};

const auto &vga_font = (chariot_kernel_font &)*&build_font_ckf;

#define VCONSOLE_WIDTH 640
#define VCONSOLE_HEIGHT 480

#define EDGE_MARGIN 0
#define TOTAL_MARGIN (EDGE_MARGIN * 2)
#define FONT_WIDTH (7)  // hardcoded :/
#define FONT_HEIGHT (13)

#define CHAR_LINE_MARGIN (3)
#define LINE_HEIGHT (FONT_HEIGHT + CHAR_LINE_MARGIN)

#define VC_COLS ((VCONSOLE_WIDTH - TOTAL_MARGIN) / FONT_WIDTH)
#define VC_ROWS ((VCONSOLE_HEIGHT - TOTAL_MARGIN) / LINE_HEIGHT)

static void vga_char_scribe(int col, int row, struct vc_cell *, int flags);

// manually create
struct vc_cell static_cells[VC_COLS * VC_ROWS];
struct vcons vga_console {
  .state = 0, .cols = VC_COLS, .rows = VC_ROWS, .pos = 0, .x = 0, .y = 0,
  .saved_x = 0, .saved_y = 0, .scribe = vga_char_scribe, .npar = 0, .ques = 0,
  .attr = 0x07, .buf = (struct vc_cell *)&static_cells,
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

static void flush_vga_console() {
  for (int y = 0; y < VC_ROWS; y++) {
    for (int x = 0; x < VC_COLS; x++) {
      int cursor = vga_console.x == x && vga_console.y == y;
      vga_char_scribe(x, y, vc_get_cell(&vga_console, x, y), cursor);
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
  set_register(VBE_DISPI_INDEX_ENABLE,
               VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
  set_register(VBE_DISPI_INDEX_BANK, 0);

  info.active = i.active;
  info.width = i.width;
  info.height = i.height;
}


























#define SVGA_IO_BASE (vmware_io)
#define SVGA_IO_MUL 1
#define SVGA_INDEX_PORT 0
#define SVGA_VALUE_PORT 1





/*
 * Registers
 */

enum {
  SVGA_REG_ID = 0,
  SVGA_REG_ENABLE = 1,
  SVGA_REG_WIDTH = 2,
  SVGA_REG_HEIGHT = 3,
  SVGA_REG_MAX_WIDTH = 4,
  SVGA_REG_MAX_HEIGHT = 5,
  SVGA_REG_DEPTH = 6,
  SVGA_REG_BITS_PER_PIXEL = 7, /* Current bpp in the guest */
  SVGA_REG_PSEUDOCOLOR = 8,
  SVGA_REG_RED_MASK = 9,
  SVGA_REG_GREEN_MASK = 10,
  SVGA_REG_BLUE_MASK = 11,
  SVGA_REG_BYTES_PER_LINE = 12,
  SVGA_REG_FB_START = 13, /* (Deprecated) */
  SVGA_REG_FB_OFFSET = 14,
  SVGA_REG_VRAM_SIZE = 15,
  SVGA_REG_FB_SIZE = 16,

  /* ID 0 implementation only had the above registers, then the palette */
  SVGA_REG_ID_0_TOP = 17,

  SVGA_REG_CAPABILITIES = 17,
  SVGA_REG_MEM_START = 18, /* (Deprecated) */
  SVGA_REG_MEM_SIZE = 19,
  SVGA_REG_CONFIG_DONE = 20,         /* Set when memory area configured */
  SVGA_REG_SYNC = 21,                /* See "FIFO Synchronization Registers" */
  SVGA_REG_BUSY = 22,                /* See "FIFO Synchronization Registers" */
  SVGA_REG_GUEST_ID = 23,            /* (Deprecated) */
  SVGA_REG_CURSOR_ID = 24,           /* (Deprecated) */
  SVGA_REG_CURSOR_X = 25,            /* (Deprecated) */
  SVGA_REG_CURSOR_Y = 26,            /* (Deprecated) */
  SVGA_REG_CURSOR_ON = 27,           /* (Deprecated) */
  SVGA_REG_HOST_BITS_PER_PIXEL = 28, /* (Deprecated) */
  SVGA_REG_SCRATCH_SIZE = 29,        /* Number of scratch registers */
  SVGA_REG_MEM_REGS = 30,            /* Number of FIFO registers */
  SVGA_REG_NUM_DISPLAYS = 31,        /* (Deprecated) */
  SVGA_REG_PITCHLOCK = 32,           /* Fixed pitch for all modes */
  SVGA_REG_IRQMASK = 33,             /* Interrupt mask */

  /* Legacy multi-monitor support */
  SVGA_REG_NUM_GUEST_DISPLAYS =
      34, /* Number of guest displays in X/Y direction */
  SVGA_REG_DISPLAY_ID =
      35, /* Display ID for the following display attributes */
  SVGA_REG_DISPLAY_IS_PRIMARY = 36, /* Whether this is a primary display */
  SVGA_REG_DISPLAY_POSITION_X = 37, /* The display position x */
  SVGA_REG_DISPLAY_POSITION_Y = 38, /* The display position y */
  SVGA_REG_DISPLAY_WIDTH = 39,      /* The display's width */
  SVGA_REG_DISPLAY_HEIGHT = 40,     /* The display's height */

  /* See "Guest memory regions" below. */
  SVGA_REG_GMR_ID = 41,
  SVGA_REG_GMR_DESCRIPTOR = 42,
  SVGA_REG_GMR_MAX_IDS = 43,
  SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH = 44,

  SVGA_REG_TRACES = 45, /* Enable trace-based updates even when FIFO is on */
  SVGA_REG_GMRS_MAX_PAGES = 46, /* Maximum number of 4KB pages for all GMRs */
  SVGA_REG_MEMORY_SIZE = 47,  /* Total dedicated device memory excluding FIFO */
  SVGA_REG_COMMAND_LOW = 48,  /* Lower 32 bits and submits commands */
  SVGA_REG_COMMAND_HIGH = 49, /* Upper 32 bits of command buffer PA */

  /*
   * Max primary memory.
   * See SVGA_CAP_NO_BB_RESTRICTION.
   */
  SVGA_REG_MAX_PRIMARY_MEM = 50,
  SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM = 50,

  SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB = 51, /* Sugested limit on mob mem */
  SVGA_REG_DEV_CAP = 52, /* Write dev cap index, read value */
  SVGA_REG_CMD_PREPEND_LOW = 53,
  SVGA_REG_CMD_PREPEND_HIGH = 54,
  SVGA_REG_SCREENTARGET_MAX_WIDTH = 55,
  SVGA_REG_SCREENTARGET_MAX_HEIGHT = 56,
  SVGA_REG_MOB_MAX_SIZE = 57,
  SVGA_REG_BLANK_SCREEN_TARGETS = 58,
  SVGA_REG_CAP2 = 59,
  SVGA_REG_DEVEL_CAP = 60,
  SVGA_REG_TOP = 61, /* Must be 1 more than the last register */

  SVGA_PALETTE_BASE = 1024, /* Base of SVGA color map */
  /* Next 768 (== 256*3) registers exist for colormap */
  // SVGA_SCRATCH_BASE = SVGA_PALETTE_BASE + SVGA_NUM_PALETTE_REGS
  /* Base of scratch registers */
  /* Next reg[SVGA_REG_SCRATCH_SIZE] registers exist for scratch usage:
     First 4 are reserved for VESA BIOS Extension; any remaining are for
     the use of the current SVGA driver. */
};

static uint32_t vmware_io = 0;


/*
static void vmware_write(int reg, int value) {
  outl(SVGA_IO_MUL * SVGA_INDEX_PORT + SVGA_IO_BASE, reg);
  outl(SVGA_IO_MUL * SVGA_VALUE_PORT + SVGA_IO_BASE, value);
}
*/

static uint32_t vmware_read(int reg) {
  outl(SVGA_IO_MUL * SVGA_INDEX_PORT + SVGA_IO_BASE, reg);
  return inl(SVGA_IO_MUL * SVGA_VALUE_PORT + SVGA_IO_BASE);
}






















// static pci::device *vga_dev = NULL;

static void *get_framebuffer_address(void) {
  void *addr = nullptr;
  pci::walk_devices([&](pci::device *dev) {


    // vmware SVGA II Adapter
    if (dev->is_device(0x15ad, 0x0405)) {
      uint32_t t = dev->get_bar(0).raw;
      vmware_io = (t & 0xFFFFFFF0);
      printk(KERN_DEBUG "vmware_io: 0x%x\n", vmware_io);
			addr = (void*)(size_t)vmware_read(SVGA_REG_FB_START);
		// auto fb = (uint32_t *)p2v(fb_addr);
    }

    if (dev->is_device(0x1234, 0x1111) || dev->is_device(0x80ee, 0xbeef)) {
      // vga_dev = dev;
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
  // pre-cast (dunno if this is dangerous)


  switch (cmd) {
    case FB_SET_XOFF:
      set_register(VBE_DISPI_INDEX_X_OFFSET, (u16)arg);
      return 0;

    case FB_SET_YOFF:
      set_register(VBE_DISPI_INDEX_Y_OFFSET, (u16)arg);
      return 0;

    case FB_SET_INFO:
      if (!curproc->mm->validate_struct<struct ck_fb_info>(
              arg, VALIDATE_READ | VALIDATE_WRITE)) {
        return -1;
      }
      set_info(*(struct ck_fb_info *)arg);
      return 0;

      break;

    case FB_GET_INFO:
      if (!curproc->mm->validate_struct<struct ck_fb_info>(
              arg, VALIDATE_READ | VALIDATE_WRITE)) {
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
  scoped_lock l(fblock);
  // printk(KERN_INFO "[fb] open\n");
  if (owned) return -EBUSY;  // disallow
  owned = true;
  return 0;  // allow
}

void vga::configure(struct ck_fb_info &i) {
  if (i.active == false) {
    i.width = VCONSOLE_WIDTH;
    i.height = VCONSOLE_HEIGHT;
  }
  set_info(i);
  flush_vga_console();
}

static void reset_fb(
    void) {  // disable the framebuffer (drop back to text mode)
  auto i = info;
  i.active = false;
  i.width = VCONSOLE_WIDTH;
  i.height = VCONSOLE_HEIGHT;
  vga::configure(i);
}

static void fb_close(fs::file &f) {
  scoped_lock l(fblock);
  // printk(KERN_INFO "[fb] close\n");
  owned = false;
  reset_fb();
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




static ref<mm::vmobject> vga_mmap(fs::file &f, size_t npages, int prot,
                                  int flags, off_t off) {
  // XXX: this is invalid, should be asserted before here :^)
  if (off != 0) {
    printk(KERN_WARN "vga: attempt to mmap at invalid offset (%d != 0)\n", off);
    return nullptr;
  }

  if (npages > NPAGES(64 * MB)) {
    printk(KERN_WARN "vga: attempt to mmap too many pages (%d pixels)\n",
           (npages * 4096) / sizeof(uint32_t));
    return nullptr;
  }


  if (flags & MAP_PRIVATE) {
    printk(KERN_WARN
           "vga: attempt to mmap with MAP_PRIVATE doesn't make sense :^)\n");
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

void vga::early_init(void) {
  if (vga_fba == NULL) vga_fba = (u32 *)get_framebuffer_address();

  if (vga_fba != NULL) {
    reset_fb();
    set_register(VBE_DISPI_INDEX_X_OFFSET, 0);
    set_register(VBE_DISPI_INDEX_Y_OFFSET, 0);
    cons_enabled = true;
  }
}

static int fg_colors[] = {
    0x676767, 0xff6d67, 0x59f68d, 0xf3f89d,
    0xc9a8fa, 0xff92d0, 0x99ecfd, 0xfeffff,
};

static int bg_colors[] = {
    0x000000, 0xff6d67, 0x59f68d, 0xf3f89d,
    0xc9a8fa, 0xff92d0, 0x99ecfd, 0xc7c7c7,
};

static void vga_char_scribe(int x, int y, struct vc_cell *cell, int flags) {
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

void vga_mod_init(void) {
  if (vga_fba != NULL) {
    printk(KERN_INFO "Standard VGA Framebuffer found!\n");
    dev::register_driver(generic_driver_info);
    dev::register_name(generic_driver_info, "fb", 0);
  }
}

module_init("vga framebuffer", vga_mod_init);
