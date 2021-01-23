#pragma once

#include <gfx/rect.h>
#include <stdint.h>
#include <stdlib.h>

#define LUMEN_MAGIC 0x5D4c


// all names are this long
#define LUMEN_NAMESZ 256

namespace lumen {

  // messages with F set at the top are designed to be sent to the window
  // server, not the client handler
#define FOR_WINDOW_SERVER 0xF000

  // first message that is to be sent
#define LUMEN_MSG_GREET (0 | FOR_WINDOW_SERVER)
#define LUMEN_MSG_GREETBACK (1)

#define LUMEN_MSG_CREATE_WINDOW (2 | FOR_WINDOW_SERVER)
#define LUMEN_MSG_WINDOW_CREATED (3)

// invalidate a region of the window
#define LUMEN_MSG_WINDOW_INVALIDATE (4 | FOR_WINDOW_SERVER)
#define LUMEN_MSG_WINDOW_INVALIDATED (4)


// Tell the window to resize to a new bitmap
#define LUMEN_MSG_RESIZE (5 | FOR_WINDOW_SERVER)
#define LUMEN_MSG_RESIZED (5)

#define LUMEN_MSG_INPUT (3)


#define LUMEN_MSG_MOVEREQ (7 | FOR_WINDOW_SERVER)

  struct msg {
    unsigned short magic = LUMEN_MAGIC;
    unsigned short type = -1;


    // "unique" id for this message. That way we know who to notify
    // when a response is gotten.
    unsigned long id;
    unsigned short window_id = -1;  // -1 means no window
    int len = 0;                    // the size of data[]

    char data[];
  };

  struct greet_msg {
    int pid;  // the pid of the process
  };

#define LUMEN_GREETBACK_MAGIC 0xF00D
  struct greetback_msg {
    unsigned short magic;
    int guest_id;
  };

  struct create_window_msg {
    int width, height;

    char name[LUMEN_NAMESZ];
  };

  struct window_created_msg {
    // if this is -1, it failed
    int window_id;
    char bitmap_name[LUMEN_NAMESZ];
  };


#define LUMEN_INPUT_KEYBOARD 1
#define LUMEN_INPUT_MOUSE 2

#define LUMEN_MOUSE_LEFT_CLICK 0x01
#define LUMEN_MOUSE_RIGHT_CLICK 0x02
#define LUMEN_MOUSE_MIDDLE_CLICK 0x04
#define LUMEN_MOUSE_SCROLL_DOWN 0x08
#define LUMEN_MOUSE_SCROLL_UP 0x10


#define LUMEN_KBD_MOD_NONE 0x00
#define LUMEN_KBD_MOD_ALT 0x01
#define LUMEN_KBD_MOD_CTRL 0x02
#define LUMEN_KBD_MOD_SHIFT 0x04
#define LUMEN_KBD_MOD_SUPER 0x08
#define LUMEN_KBD_MOD_SHIFT 0x04
#define LUMEN_KBD_MOD_MASK 0x0F

#define LUMEN_KBD_PRESS 0x80



  struct input_msg {
    unsigned short type;

    int window_id;
    union {
      // mouse data
      struct {
        int16_t dx, dy; /* Move delta */
        int16_t hx, hy; /* Hover locaiton */
        uint8_t buttons; /* The current buttons that are currently active */
      } mouse;

      struct {
        unsigned char keycode;
        unsigned char c;  // the ascii char of this keystroke
        unsigned char flags;
      } keyboard;
    };
  };


#define MAX_INVALIDATE 10
  struct invalidate_msg {
    int id;  // window id
    int nrects;
    bool sync; /* do I expect a response message? */
    struct r {
      // where in the window?
      int x, y, w, h;
    } rects[MAX_INVALIDATE];
  };

  struct invalidated_msg {
    int id;  // window id;
  };



  struct resize_msg {
    int id;  // window id
    int width, height;
  };

  struct resized_msg {
    int id;  // window id
    // confirmation
    int width, height;
    char bitmap_name[LUMEN_NAMESZ];
  };


  /* Ask the compositor to move the window. This can be ignored. */
  struct move_request {
    int id;  // window id
    /* The delta to move. */
    int dx, dy;
  };

}  // namespace lumen
