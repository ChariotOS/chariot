#pragma once

#include <types.h>

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

namespace vga {

enum color : char {
  black = 0,
  blue = 1,
  green = 2,
  cyan = 3,
  red = 4,
  magenta = 5,
  brown = 6,
  light_grey = 7,
  dark_grey = 8,
  light_blue = 9,
  light_green = 10,
  light_cyan = 11,
  light_red = 12,
  light_magenta = 13,
  light_brown = 14,
  white = 15,
};

u16 make_entry(char c, uint8_t attr);
u8 make_color(enum vga::color fg, enum vga::color bg);
void set_cursor(uint8_t x, uint8_t y);
void get_cursor(uint8_t *x, uint8_t *y);

void putchar(char c);

void init();

void scrollup();

void set_color(enum vga::color fg, enum vga::color bg);
void clear_screen(uint16_t val);
void clear_screen(char, vga::color);
void clear_screen();

void set_pixel(int ind, int color);
void set_pixel(int x, int y, int color);

int rgb(int r, int g, int b);

int hsl(double h, double s, double l);

int width();
int height();

int flush_buffer(u32 *, int npixels);

static inline u64 npixels(void) {
  return width() * height();
}

};  // namespace vga
