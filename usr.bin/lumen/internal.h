#pragma once

#include <chariot/fb.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <ck/io.h>
#include <ck/socket.h>
#include <lumen/msg.h>
#include <chariot/keycode.h>
#include <chariot/mouse_packet.h>
#include "gfx/rect.h"

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


	struct window {
		int id = 0;
		ck::string name;
		gfx::rect rect;

	};



	// a client who is connected to the server
	// Clients can have many windows
	struct client {
		long id = 0;
		struct context &ctx; // the context we live in
		ck::localsocket *connection;
		ck::map<long, struct window *> windows;

		client(long id, struct context &ctx, ck::localsocket *conn);
		~client(void);

		void process_message(lumen::msg &);
		void on_read(void);

		long send_raw(int type, int id, void *payload, size_t payloadsize);

    template <typename T>
    inline int send_msg(int type, const T &payload) {
			int id = next_msg_id;
      return send_raw(type, id, (void *)&payload, sizeof(payload));
    }


    template <typename T>
    inline int respond(lumen::msg &m, int type, const T &payload) {
      return send_raw(type, m.id, (void *)&payload, sizeof(payload));
    }



		struct window *new_window(ck::string name, gfx::rect r);


		// a big number.
		long next_window_id = 0;
		long next_msg_id = 0x7F00'0000'0000'0000;
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
		void process_message(lumen::client &c, lumen::msg &);
		void client_closed(long id);

		private:
			ck::map<long, lumen::client *> clients;
			long next_client_id = 0;

	};


}  // namespace lumen
