#pragma once

#include <chariot/fb.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <ck/io.h>
#include <ck/socket.h>

#include <chariot/keycode.h>
#include <chariot/mouse_packet.h>

namespace lumen {

  // represents a framebuffer for a screen
  class screen {
    int fd = -1;
    size_t bufsz = 0;
    uint32_t *buf = NULL;
    struct ck_fb_info info;

    void flush_info(void) {
      info.active = 1;
      ioctl(fd, FB_SET_INFO, &info);
    }

    void load_info(void) { ioctl(fd, FB_GET_INFO, &info); }

   public:
    screen(int w, int h);
    ~screen(void);

    void set_resolution(int w, int h);
    inline size_t screensize(void) { return bufsz; }
    inline uint32_t *pixels(void) { return buf; }

		inline int width(void) {return info.width;}
		inline int height(void) {return info.height;}
  };



	/**
	 * contains all the state needed to run the window server
	 */
	struct context {


		lumen::screen screen;

		ck::file keyboard, mouse;
		ck::localsocket server;

		context(void);



		void accept_connection(void);
		void handle_keyboard_input(keyboard_packet_t &pkt);
		void handle_mouse_input(struct mouse_packet &pkt);


		private:
			ck::map<long, ck::localsocket *> clients;
			long next_client_id = 0;

	};


}  // namespace lumen
