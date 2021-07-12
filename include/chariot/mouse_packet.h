#pragma once



#define MOUSE_MAGIC 0xFEED1234
#define MOUSE_LEFT_CLICK 0x01
#define MOUSE_RIGHT_CLICK 0x02
#define MOUSE_MIDDLE_CLICK 0x04
#define MOUSE_SCROLL_DOWN 0x08
#define MOUSE_SCROLL_UP 0x10

struct mouse_packet {
  unsigned int magic;
  int x;
  int y;
  char is_relative;
  unsigned int buttons;
  // unsigned long timestamp;  // when?;
};
