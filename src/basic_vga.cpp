#include <asm.h>
#include <module.h>
#include <printk.h>


#include <vga.h>


static u8 vga_x, vga_y;
static u8 vga_attr;


static inline void vga_write_screen(uint8_t x, uint8_t y, uint16_t val) {
  *(((uint16_t *)VGA_BASE_ADDR) + y * VGA_WIDTH + x) = val;
}

void vga::clear_screen(char val, vga::color color) {
  vga::clear_screen(make_entry(val, color));
}

void vga::clear_screen(uint16_t val) {
  int i;
  for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    *(((uint16_t *)VGA_BASE_ADDR) + i) = val;
  }
  vga_x = 0;
  vga_y = 0;
}

void vga::clear_screen(void) {
  vga::clear_screen(make_entry(' ', vga_attr));
}

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
  uint16_t *buf = (uint16_t *)VGA_BASE_ADDR;

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
  vga_attr = vga::make_color(color::white, color::blue);
  vga::clear_screen(vga::make_entry(' ', vga_attr));
  vga::set_cursor(vga_x, vga_y);
}

