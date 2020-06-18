#pragma once

#include <gfx/rect.h>
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

#define LUMEN_MSG_INPUT (3)


  struct msg {
    unsigned short magic = LUMEN_MAGIC;
    unsigned short type = -1;

    // length of the payload
    int len = 0;
		int window_id = -1; // -1 means no window

    // "unique" id for this message. That way we know who to notify
    // when a response is gotten.
    unsigned long id;

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
		//
	};


#define LUMEN_INPUT_KEYBOARD 1
#define LUMEN_INPUT_MOUSE 2


#define LUMEN_MOUSE_LEFT_CLICK 0x01
#define LUMEN_MOUSE_RIGHT_CLICK 0x02
#define LUMEN_MOUSE_MIDDLE_CLICK 0x04
#define LUMEN_MOUSE_SCROLL_DOWN 0x08
#define LUMEN_MOUSE_SCROLL_UP 0x10


	struct input_msg {
		unsigned short type;

		union {
			// mouse data
			struct {
				unsigned short xpos;
				unsigned short ypos;
				int buttons;
			} mouse;

			struct {
				unsigned char keycode;
				unsigned char c; // the ascii char of this keystroke
				unsigned char flags;
			} keyboard;
		};
	};



}  // namespace lumen
