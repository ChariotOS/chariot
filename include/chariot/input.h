#pragma once

#include <keycode.h>
#include <mouse_packet.h>


#define INPUT_KEYBOARD 1
#define INPUT_MOUSE 2

typedef struct {
  int type;  // INPUT_*
  union {
    struct keyboard_packet_t kbd;
    struct mouse_packet mouse;
  };
} input_packet_t;
