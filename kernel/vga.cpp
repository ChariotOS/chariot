#include <asm.h>
#include <dev/driver.h>
#include <mem.h>
#include <module.h>
#include <pci.h>
#include <printk.h>
#include <util.h>
#include <vga.h>
#include <cpu.h>

#include "../drivers/majors.h"

static u8 vga_x, vga_y;
static u8 vga_attr;

static inline void vga_write_screen(uint8_t x, uint8_t y, uint16_t val) {
  *(((uint16_t *)p2v(VGA_BASE_ADDR)) + y * VGA_WIDTH + x) = val;
}

void vga::clear_screen(char val, vga::color color) {
  return;
  vga::clear_screen(make_entry(val, color));
}

void vga::clear_screen(uint16_t val) {
  return;
  int i;
  for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    *(((uint16_t *)VGA_BASE_ADDR) + i) = val;
  }
  vga_x = 0;
  vga_y = 0;
}

void vga::clear_screen(void) { vga::clear_screen(make_entry(' ', vga_attr)); }

static inline void vga_copy_out(void *dest, uint32_t n) {
  memcpy((void *)dest, (void *)VGA_BASE_ADDR, n);
}

static inline void vga_copy_in(void *src, uint32_t n) {
  memcpy((void *)VGA_BASE_ADDR, src, n);
}

u16 vga::make_entry(char c, uint8_t color) {
  uint16_t c16 = c;
  uint16_t color16 = color;
  return c16 | color16 << 8;
}

u8 vga::make_color(enum vga::color fg, enum vga::color bg) {
  return fg | bg << 4;
}

void vga::putchar(char c) {
  if (c == '\n') {
    vga_x = 0;
    if (++vga_y == VGA_HEIGHT) {
      vga::scrollup();
      vga_y--;
    }
  } else {
    vga_write_screen(vga_x, vga_y, vga::make_entry(c, vga_attr));

    if (++vga_x == VGA_WIDTH) {
      vga_x = 0;
      if (++vga_y == VGA_HEIGHT) {
        vga::scrollup();
        vga_y--;
      }
    }
  }
  vga::set_cursor(vga_x, vga_y);
}

inline void vga::set_cursor(uint8_t x, uint8_t y) {
  uint16_t pos = y * VGA_WIDTH + x;

  vga_x = x;
  vga_y = y;

  outb(CRTC_ADDR, CURSOR_HIGH);
  outb(CRTC_DATA, pos >> 8);
  outb(CRTC_DATA, CURSOR_LOW);
  outb(CRTC_DATA, pos & 0xff);
}

inline void vga::get_cursor(uint8_t *x, uint8_t *y) {
  *x = vga_x;
  *y = vga_y;
}

void vga::scrollup(void) {
  int i;
  uint16_t *buf = (uint16_t *)p2v(VGA_BASE_ADDR);

  for (i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
    buf[i] = buf[i + VGA_WIDTH];
  }

  for (i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
    buf[i] = vga::make_entry(' ', vga_attr);
  }
}

void vga::set_color(enum vga::color fg, enum vga::color bg) {
  vga_attr = make_color(fg, bg);
}

void vga::init(void) {
  vga_x = vga_y = 0;
  vga_attr = vga::make_color(color::white, color::black);
  vga::clear_screen(vga::make_entry(' ', vga_attr));
  vga::set_cursor(vga_x, vga_y);
}

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

struct BXVGAResolution {
  int width;
  int height;
};

u32 *vga_fba = 0;
int m_framebuffer_width = 0;
int m_framebuffer_height = 0;

int vga::width() { return m_framebuffer_width; }

int vga::height() { return m_framebuffer_height; }

static void set_register(u16 index, u16 data) {
  outw(VBE_DISPI_IOPORT_INDEX, index);
  outw(VBE_DISPI_IOPORT_DATA, data);
}

static void set_resolution(int width, int height) {
  m_framebuffer_width = width;
  m_framebuffer_height = height;

  set_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
  set_register(VBE_DISPI_INDEX_XRES, (u16)width);
  set_register(VBE_DISPI_INDEX_YRES, (u16)height);
  set_register(VBE_DISPI_INDEX_VIRT_WIDTH, (u16)width);
  set_register(VBE_DISPI_INDEX_VIRT_HEIGHT, (u16)height * 2);
  // bits per pixel
  set_register(VBE_DISPI_INDEX_BPP, 32);
  set_register(VBE_DISPI_INDEX_ENABLE,
               VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
  set_register(VBE_DISPI_INDEX_BANK, 0);
}

void vga::set_pixel(int x, int y, int color) {
  if (vga_fba == 0) return;
  if (x >= m_framebuffer_width || y >= m_framebuffer_height) return;

  set_pixel(y * m_framebuffer_width + x, color);
}

void vga::set_pixel(int ind, int color) {
  if (vga_fba == 0) return;
  if (ind >= m_framebuffer_width * m_framebuffer_height) return;

  vga_fba[ind] = color;
}

int vga::rgb(int r, int g, int b) {
  return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
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

int vga::flush_buffer(u32 *dbuf, int npixels) {

  cpu::pushcli();
  int len = width() * height();
  if (npixels < len) len = npixels;
  int i = 0;
  for (; i < npixels; i += 2) *(u64 *)(vga_fba + i) = *(u64 *)(dbuf + i);
  for (; i < npixels; i++) vga_fba[i] = dbuf[i];

  cpu::popcli();
  return len;
}

/**
 * give the user access to writing the framebuffer
 */
static ssize_t fb_write(fs::filedesc &fd, const char *buf, size_t sz) {
  if (fd) {
    if (vga_fba == nullptr) return -1;

    size_t fbsize = vga::npixels() * sizeof(u32);
    auto off = fd.offset() % fbsize;
    ssize_t space_left = fbsize - off;

    ssize_t to_copy = min(space_left, sz);
    if (to_copy <= 0) return 0;

    cpu::pushcli();
    memcpy((char *)vga_fba + off, buf, to_copy);
    cpu::popcli();

    // seek past
    fd.seek(sz, SEEK_CUR);
    return sz;
  }
  return -1;
}

static int fb_ioctl(fs::filedesc &fd, unsigned int cmd, unsigned long arg) {
  return -1;
}

// can only write to the framebuffer
struct dev::driver_ops fb_ops = {
    .llseek = NULL,
    .read = NULL,
    .write = fb_write,
    .ioctl = fb_ioctl,

    .open =  NULL,
    .close = NULL,
};

static void vga_init_mod(void) {
  vga_fba = (u32 *)p2v(get_framebuffer_address());

  vga_dev->enable_bus_mastering();

  // set_resolution(1366, 768);

  // set_resolution(640, 480);
  set_resolution(800, 600);
  /*
  */
  dev::register_driver("fb", CHAR_DRIVER, MAJOR_FB, &fb_ops);
}

module_init("vga", vga_init_mod);
