/**
 * A driver written for the Vmware SVGA II adapter. Not at all
 * sure if it's good or not
 */

#include <mem.h>
#include <module.h>
#include <pci.h>



#define SVGA_CAP_NONE 0x00000000
#define SVGA_CAP_RECT_COPY 0x00000002
#define SVGA_CAP_CURSOR 0x00000020
#define SVGA_CAP_CURSOR_BYPASS 0x00000040
#define SVGA_CAP_CURSOR_BYPASS_2 0x00000080
#define SVGA_CAP_8BIT_EMULATION 0x00000100
#define SVGA_CAP_ALPHA_CURSOR 0x00000200
#define SVGA_CAP_3D 0x00004000
#define SVGA_CAP_EXTENDED_FIFO 0x00008000
#define SVGA_CAP_MULTIMON 0x00010000
#define SVGA_CAP_PITCHLOCK 0x00020000
#define SVGA_CAP_IRQMASK 0x00040000
#define SVGA_CAP_DISPLAY_TOPOLOGY 0x00080000
#define SVGA_CAP_GMR 0x00100000
#define SVGA_CAP_TRACES 0x00200000
#define SVGA_CAP_GMR2 0x00400000
#define SVGA_CAP_SCREEN_OBJECT_2 0x00800000
#define SVGA_CAP_COMMAND_BUFFERS 0x01000000
#define SVGA_CAP_DEAD1 0x02000000
#define SVGA_CAP_CMD_BUFFERS_2 0x04000000
#define SVGA_CAP_GBOBJECTS 0x08000000
#define SVGA_CAP_DX 0x10000000
#define SVGA_CAP_HP_CMD_QUEUE 0x20000000
#define SVGA_CAP_NO_BB_RESTRICTION 0x40000000
#define SVGA_CAP_CAP2_REGISTER 0x80000000


static void vmw_print_capabilities(uint32_t capabilities) {
  printf(KERN_DEBUG "Capabilities:\n");
  if (capabilities & SVGA_CAP_RECT_COPY) printf(KERN_DEBUG "  Rect copy.\n");
  if (capabilities & SVGA_CAP_CURSOR) printf(KERN_DEBUG "  Cursor.\n");
  if (capabilities & SVGA_CAP_CURSOR_BYPASS) printf(KERN_DEBUG "  Cursor bypass.\n");
  if (capabilities & SVGA_CAP_CURSOR_BYPASS_2) printf(KERN_DEBUG "  Cursor bypass 2.\n");
  if (capabilities & SVGA_CAP_8BIT_EMULATION) printf(KERN_DEBUG "  8bit emulation.\n");
  if (capabilities & SVGA_CAP_ALPHA_CURSOR) printf(KERN_DEBUG "  Alpha cursor.\n");
  if (capabilities & SVGA_CAP_3D) printf(KERN_DEBUG "  3D.\n");
  if (capabilities & SVGA_CAP_EXTENDED_FIFO) printf(KERN_DEBUG "  Extended Fifo.\n");
  if (capabilities & SVGA_CAP_MULTIMON) printf(KERN_DEBUG "  Multimon.\n");
  if (capabilities & SVGA_CAP_PITCHLOCK) printf(KERN_DEBUG "  Pitchlock.\n");
  if (capabilities & SVGA_CAP_IRQMASK) printf(KERN_DEBUG "  Irq mask.\n");
  if (capabilities & SVGA_CAP_DISPLAY_TOPOLOGY) printf(KERN_DEBUG "  Display Topology.\n");
  if (capabilities & SVGA_CAP_GMR) printf(KERN_DEBUG "  GMR.\n");
  if (capabilities & SVGA_CAP_TRACES) printf(KERN_DEBUG "  Traces.\n");
  if (capabilities & SVGA_CAP_GMR2) printf(KERN_DEBUG "  GMR2.\n");
  if (capabilities & SVGA_CAP_SCREEN_OBJECT_2) printf(KERN_DEBUG "  Screen Object 2.\n");
  if (capabilities & SVGA_CAP_COMMAND_BUFFERS) printf(KERN_DEBUG "  Command Buffers.\n");
  if (capabilities & SVGA_CAP_CMD_BUFFERS_2) printf(KERN_DEBUG "  Command Buffers 2.\n");
  if (capabilities & SVGA_CAP_GBOBJECTS) printf(KERN_DEBUG "  Guest Backed Resources.\n");
  if (capabilities & SVGA_CAP_DX) printf(KERN_DEBUG "  DX Features.\n");
  if (capabilities & SVGA_CAP_HP_CMD_QUEUE) printf(KERN_DEBUG "  HP Command Queue.\n");
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
  SVGA_REG_NUM_GUEST_DISPLAYS = 34, /* Number of guest displays in X/Y direction */
  SVGA_REG_DISPLAY_ID = 35,         /* Display ID for the following display attributes */
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

  SVGA_REG_TRACES = 45,         /* Enable trace-based updates even when FIFO is on */
  SVGA_REG_GMRS_MAX_PAGES = 46, /* Maximum number of 4KB pages for all GMRs */
  SVGA_REG_MEMORY_SIZE = 47,    /* Total dedicated device memory excluding FIFO */
  SVGA_REG_COMMAND_LOW = 48,    /* Lower 32 bits and submits commands */
  SVGA_REG_COMMAND_HIGH = 49,   /* Upper 32 bits of command buffer PA */

  /*
   * Max primary memory.
   * See SVGA_CAP_NO_BB_RESTRICTION.
   */
  SVGA_REG_MAX_PRIMARY_MEM = 50,
  SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM = 50,

  SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB = 51, /* Sugested limit on mob mem */
  SVGA_REG_DEV_CAP = 52,                        /* Write dev cap index, read value */
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


static void vmware_write(int reg, int value) {
  outl(SVGA_IO_MUL * SVGA_INDEX_PORT + SVGA_IO_BASE, reg);
  outl(SVGA_IO_MUL * SVGA_VALUE_PORT + SVGA_IO_BASE, value);
}

static uint32_t vmware_read(int reg) {
  outl(SVGA_IO_MUL * SVGA_INDEX_PORT + SVGA_IO_BASE, reg);
  return inl(SVGA_IO_MUL * SVGA_VALUE_PORT + SVGA_IO_BASE);
}



static void vmware_set_resolution(uint16_t w, uint16_t h) {
  /*
vmware_write(SVGA_REG_ENABLE, 1);
// vmware_write(SVGA_REG_ID, 0);
vmware_write(SVGA_REG_WIDTH, w);
vmware_write(SVGA_REG_HEIGHT, h);
vmware_write(SVGA_REG_BITS_PER_PIXEL, 32);
vmware_write(SVGA_REG_ENABLE, 1);
  */


  vmware_write(SVGA_REG_ENABLE, 1);
  vmware_write(SVGA_REG_DISPLAY_ID, 0);
  vmware_write(SVGA_REG_DISPLAY_IS_PRIMARY, 1);
  vmware_write(SVGA_REG_DISPLAY_POSITION_X, 0);
  vmware_write(SVGA_REG_DISPLAY_POSITION_Y, 0);
  vmware_write(SVGA_REG_WIDTH, w);
  vmware_write(SVGA_REG_HEIGHT, h);

  // vmware_write(SVGA_REG_DISPLAY_ID, SVGA_ID_INVALID);

  uint32_t bpl = vmware_read(SVGA_REG_BYTES_PER_LINE);

  printf(KERN_DEBUG "bpl: %d\n", bpl);

  // lfb_resolution_x = w;
  // lfb_resolution_s = bpl;
  // lfb_resolution_y = h;
  // lfb_resolution_b = 32;
}

void svga_mod_init(void) {
  return;
  // void *addr = nullptr;
  pci::walk_devices([&](pci::device *dev) {
    // vmware SVGA II Adapter
    if (dev->is_device(0x15ad, 0x0405)) {
      uint32_t t = dev->get_bar(0).raw;
      vmware_io = (t & 0xFFFFFFF0);
      printf(KERN_DEBUG "vmware_io: 0x%x\n", vmware_io);
    }
  });


  uint32_t fb_addr = vmware_read(SVGA_REG_FB_START);
  auto fb = (uint32_t *)p2v(fb_addr);
  debug("vmware fb address: 0x%p\n", fb);
  uint32_t fb_size = vmware_read(15);

  debug("vmware fb size: 0x%x\n", fb_size);

  debug("id: 0x%08x\n", vmware_read(SVGA_REG_ID));

  debug("vram size: %d\n", vmware_read(SVGA_REG_VRAM_SIZE));
  ;
  debug("mmio size: %d\n", vmware_read(SVGA_REG_MEM_SIZE));
  debug("max width: %d\n", vmware_read(SVGA_REG_MAX_WIDTH));
  debug("max height: %d\n", vmware_read(SVGA_REG_MAX_HEIGHT));


  auto cap = vmware_read(17);  // read the capabilities
  vmw_print_capabilities(cap);

  vmware_set_resolution(640, 480);
  for (int i = 0; i < 640 * 480; i++) {
    fb[i] = i;
  }
}
module_init("svga II framebuffer", svga_mod_init);
