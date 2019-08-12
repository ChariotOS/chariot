#include <asm.h>
#include <module.h>
#include <printk.h>

#define VGA_BASE_ADDR 0xb8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define CRTC_ADDR 0x3d4
#define CRTC_DATA 0x3d5
#define CURSOR_LOW 0xf
#define CURSOR_HIGH 0xe

#define ATTR_CTRL_FLIP_FLOP 0x3da
#define ATTR_CTRL_ADDR_AND_DATA_WRITE 0x3c0
#define ATTR_CTRL_DATA_READ 0x3c1
#define ATTR_MODE_CTRL 0x10

enum vga_color {
  COLOR_BLACK = 0,
  COLOR_BLUE = 1,
  COLOR_GREEN = 2,
  COLOR_CYAN = 3,
  COLOR_RED = 4,
  COLOR_MAGENTA = 5,
  COLOR_BROWN = 6,
  COLOR_LIGHT_GREY = 7,
  COLOR_DARK_GREY = 8,
  COLOR_LIGHT_BLUE = 9,
  COLOR_LIGHT_GREEN = 10,
  COLOR_LIGHT_CYAN = 11,
  COLOR_LIGHT_RED = 12,
  COLOR_LIGHT_MAGENTA = 13,
  COLOR_LIGHT_BROWN = 14,
  COLOR_WHITE = 15,
};

uint16_t vga_make_entry(char c, uint8_t attr);
uint8_t vga_make_color(enum vga_color fg, enum vga_color bg);
void vga_set_cursor(uint8_t x, uint8_t y);
void vga_get_cursor(uint8_t *x, uint8_t *y);

void vga_early_init();
void vga_init();

void vga_init_screen();

static inline void vga_write_screen(uint8_t x, uint8_t y, uint16_t val) {
  *(((uint16_t *)VGA_BASE_ADDR) + y * VGA_WIDTH + x) = val;
}

static inline void vga_clear_screen(uint16_t val) {
  int i;
  for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    *(((uint16_t *)VGA_BASE_ADDR) + i) = val;
  }
}

void vga_scrollup(void);
void vga_writechar(char c);
void vga_print(char *buf);
void vga_puts(char *buf);

static inline void vga_copy_out(void *dest, uint32_t n) {
  memcpy((void *)dest, (void *)VGA_BASE_ADDR, n);
}

static inline void vga_copy_in(void *src, uint32_t n) {
  memcpy((void *)VGA_BASE_ADDR, src, n);
}

static uint8_t vga_x, vga_y;
static uint8_t vga_attr;

uint16_t vga_make_entry(char c, uint8_t color) {
  uint16_t c16 = c;
  uint16_t color16 = color;
  return c16 | color16 << 8;
}

uint8_t vga_make_color(enum vga_color fg, enum vga_color bg) {
  return fg | bg << 4;
}

void vga_writechar(char c) {
  if (c == '\n') {
    vga_x = 0;
    if (++vga_y == VGA_HEIGHT) {
      vga_scrollup();
      vga_y--;
    }
  } else {
    vga_write_screen(vga_x, vga_y, vga_make_entry(c, vga_attr));

    if (++vga_x == VGA_WIDTH) {
      vga_x = 0;
      if (++vga_y == VGA_HEIGHT) {
        vga_scrollup();
        vga_y--;
      }
    }
  }
  vga_set_cursor(vga_x, vga_y);
}

inline void vga_set_cursor(uint8_t x, uint8_t y) {
  uint16_t pos = y * VGA_WIDTH + x;

  vga_x = x;
  vga_y = y;

  outb(CRTC_ADDR, CURSOR_HIGH);
  outb(CRTC_DATA, pos >> 8);
  outb(CRTC_DATA, CURSOR_LOW);
  outb(CRTC_DATA, pos & 0xff);
}

inline void vga_get_cursor(uint8_t *x, uint8_t *y) {
  *x = vga_x;
  *y = vga_y;
}

void vga_scrollup(void) {
  int i;
  uint16_t *buf = (uint16_t *)VGA_BASE_ADDR;

  for (i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
    buf[i] = buf[i + VGA_WIDTH];
  }

  for (i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
    buf[i] = vga_make_entry(' ', vga_attr);
  }
}

void vga_init(void) {
  vga_x = vga_y = 0;
  vga_attr = vga_make_color(COLOR_WHITE, COLOR_BLACK);
  vga_clear_screen(vga_make_entry(' ', vga_attr));
  vga_set_cursor(vga_x, vga_y);
}

