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

void init();
void early_init();
void late_init();
void putchar(char c);

};  // namespace vga
