#pragma once



#define MOUSE_LEFT_CLICK 0x01
#define MOUSE_RIGHT_CLICK 0x02
#define MOUSE_MIDDLE_CLICK 0x04


struct mouse_packet_t {
  unsigned int magic;
  char dx;
  char dy;
  unsigned int buttons;
};
