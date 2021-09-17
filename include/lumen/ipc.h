/**
 *  Generated by ipcc.py. Any changes will be overwritten
 *
 * Namespace: lumen
 *
 * Client messages:
 *
 *   greet (
 *     uint32_t number
 *     uint64_t time
 *   )
 *
 *   invalidate (
 *     uint32_t wid
 *     ck::ref<gfx::rect> rects
 *   )
 *
 *   open_window (
 *     int width
 *     int height
 *     ck::string window_name
 *   )
 *
 *   show_window (
 *     uint32_t wid
 *   )
 *
 *   hide_window (
 *     uint32_t wid
 *   )
 *
 *
 * Server messages:
 *
 *   window_closed (
 *     uint32_t wid
 *   )
 *
 *   mouse_event (
 *     uint32_t wid
 *     uint8_t buttons
 *     uint16_t dx
 *     uint16_t dy
 *     uint16_t hx
 *     uint16_t hy
 *   )
 *
 *   keyboard_event (
 *     uint32_t wid
 *     uint8_t keycode
 *     uint8_t c
 *     uint8_t flags
 *   )
 *
 *   window_resized (
 *     uint32_t wid
 *     int width
 *     int height
 *     ck::string new_bitmap
 *   )
 *
 */
#pragma once
#include <ck/ipc.h>
#include <ck/option.h>
#include <stdint.h>
#include <unistd.h>
#include <gfx/rect.h>


namespace lumen {
  // An enumerated identifier for each kind of message
  enum class Message : uint32_t {
    CLIENT_GREET,
    CLIENT_INVALIDATE,
    CLIENT_OPEN_WINDOW,
    CLIENT_SHOW_WINDOW,
    CLIENT_HIDE_WINDOW,
    SERVER_WINDOW_CLOSED,
    SERVER_MOUSE_EVENT,
    SERVER_KEYBOARD_EVENT,
    SERVER_WINDOW_RESIZED
  };
};  // namespace lumen



#ifndef IPC_splat_GFX_RECT
#define IPC_splat_GFX_RECT
template <>
inline bool ck::ipc::encode(ck::ipc::encoder& e, gfx::rect& v) {
  e.write((void*)&v, sizeof(gfx::rect));
  return true;
}

template <>
inline bool ck::ipc::decode(ck::ipc::decoder& d, gfx::rect& v) {
  v = d.next<gfx::rect>();
  return true;
}
#endif


namespace lumen {
  class client_connection_stub : public ck::ipc::impl::socket_connection {
   public:
    client_connection_stub(ck::ref<ck::ipcsocket> s)
        : ck::ipc::impl::socket_connection(s, "lumen", false) {}
    virtual ~client_connection_stub(void) {}

    // methods to send messages
    inline void greet(uint32_t number, uint64_t time) {
      ck::ipc::encoder __e = begin_send();
      __e << (uint32_t)lumen::Message::CLIENT_GREET;
      ck::ipc::encode(__e, number);
      ck::ipc::encode(__e, time);
      this->finish_send();
    }

    inline void invalidate(uint32_t wid, ck::ref<gfx::rect> rects) {
      ck::ipc::encoder __e = begin_send();
      __e << (uint32_t)lumen::Message::CLIENT_INVALIDATE;
      ck::ipc::encode(__e, wid);
      ck::ipc::encode(__e, rects);
      this->finish_send();
    }

    inline void open_window(int width, int height, ck::string window_name) {
      ck::ipc::encoder __e = begin_send();
      __e << (uint32_t)lumen::Message::CLIENT_OPEN_WINDOW;
      ck::ipc::encode(__e, width);
      ck::ipc::encode(__e, height);
      ck::ipc::encode(__e, window_name);
      this->finish_send();
    }

    inline void show_window(uint32_t wid) {
      ck::ipc::encoder __e = begin_send();
      __e << (uint32_t)lumen::Message::CLIENT_SHOW_WINDOW;
      ck::ipc::encode(__e, wid);
      this->finish_send();
    }

    inline void hide_window(uint32_t wid) {
      ck::ipc::encoder __e = begin_send();
      __e << (uint32_t)lumen::Message::CLIENT_HIDE_WINDOW;
      ck::ipc::encode(__e, wid);
      this->finish_send();
    }


    // handle these in your subclass
    virtual void on_window_closed(uint32_t wid) {
      fprintf(stderr, "Unimplemneted IPC handler \"on_window_closed\" on lumen client\n");
      return;
    }
    virtual void on_mouse_event(
        uint32_t wid, uint8_t buttons, uint16_t dx, uint16_t dy, uint16_t hx, uint16_t hy) {
      fprintf(stderr, "Unimplemneted IPC handler \"on_mouse_event\" on lumen client\n");
      return;
    }
    virtual void on_keyboard_event(uint32_t wid, uint8_t keycode, uint8_t c, uint8_t flags) {
      fprintf(stderr, "Unimplemneted IPC handler \"on_keyboard_event\" on lumen client\n");
      return;
    }
    virtual void on_window_resized(uint32_t wid, int width, int height, ck::string new_bitmap) {
      fprintf(stderr, "Unimplemneted IPC handler \"on_window_resized\" on lumen client\n");
      return;
    }


   protected:
    // ^ck::ipc::impl::client_connection
    virtual inline void dispatch_received_message(void* data, size_t len) {
      ck::ipc::decoder __decoder(data, len);
      lumen::Message msg_type = (lumen::Message)ck::ipc::decode<uint32_t>(__decoder);
      switch (msg_type) {
        case lumen::Message::SERVER_WINDOW_CLOSED: {
          uint32_t wid;
          ck::ipc::decode(__decoder, wid);
          on_window_closed(wid);
          break;
        }
        case lumen::Message::SERVER_MOUSE_EVENT: {
          uint32_t wid;
          uint8_t buttons;
          uint16_t dx;
          uint16_t dy;
          uint16_t hx;
          uint16_t hy;
          ck::ipc::decode(__decoder, wid);
          ck::ipc::decode(__decoder, buttons);
          ck::ipc::decode(__decoder, dx);
          ck::ipc::decode(__decoder, dy);
          ck::ipc::decode(__decoder, hx);
          ck::ipc::decode(__decoder, hy);
          on_mouse_event(wid, buttons, dx, dy, hx, hy);
          break;
        }
        case lumen::Message::SERVER_KEYBOARD_EVENT: {
          uint32_t wid;
          uint8_t keycode;
          uint8_t c;
          uint8_t flags;
          ck::ipc::decode(__decoder, wid);
          ck::ipc::decode(__decoder, keycode);
          ck::ipc::decode(__decoder, c);
          ck::ipc::decode(__decoder, flags);
          on_keyboard_event(wid, keycode, c, flags);
          break;
        }
        case lumen::Message::SERVER_WINDOW_RESIZED: {
          uint32_t wid;
          int width;
          int height;
          ck::string new_bitmap;
          ck::ipc::decode(__decoder, wid);
          ck::ipc::decode(__decoder, width);
          ck::ipc::decode(__decoder, height);
          ck::ipc::decode(__decoder, new_bitmap);
          on_window_resized(wid, width, height, new_bitmap);
          break;
        }
        default:
          panic("client: Unhandled message type %d", msg_type);
          break;
      }
    }
  };
}  // namespace lumen


namespace lumen {
  class server_connection_stub : public ck::ipc::impl::socket_connection {
   public:
    server_connection_stub(ck::ref<ck::ipcsocket> s)
        : ck::ipc::impl::socket_connection(s, "lumen", true) {}
    virtual ~server_connection_stub(void) {}

    // methods to send messages
    inline void window_closed(uint32_t wid) {
      ck::ipc::encoder __e = begin_send();
      __e << (uint32_t)lumen::Message::SERVER_WINDOW_CLOSED;
      ck::ipc::encode(__e, wid);
      this->finish_send();
    }

    inline void mouse_event(
        uint32_t wid, uint8_t buttons, uint16_t dx, uint16_t dy, uint16_t hx, uint16_t hy) {
      ck::ipc::encoder __e = begin_send();
      __e << (uint32_t)lumen::Message::SERVER_MOUSE_EVENT;
      ck::ipc::encode(__e, wid);
      ck::ipc::encode(__e, buttons);
      ck::ipc::encode(__e, dx);
      ck::ipc::encode(__e, dy);
      ck::ipc::encode(__e, hx);
      ck::ipc::encode(__e, hy);
      this->finish_send();
    }

    inline void keyboard_event(uint32_t wid, uint8_t keycode, uint8_t c, uint8_t flags) {
      ck::ipc::encoder __e = begin_send();
      __e << (uint32_t)lumen::Message::SERVER_KEYBOARD_EVENT;
      ck::ipc::encode(__e, wid);
      ck::ipc::encode(__e, keycode);
      ck::ipc::encode(__e, c);
      ck::ipc::encode(__e, flags);
      this->finish_send();
    }

    inline void window_resized(uint32_t wid, int width, int height, ck::string new_bitmap) {
      ck::ipc::encoder __e = begin_send();
      __e << (uint32_t)lumen::Message::SERVER_WINDOW_RESIZED;
      ck::ipc::encode(__e, wid);
      ck::ipc::encode(__e, width);
      ck::ipc::encode(__e, height);
      ck::ipc::encode(__e, new_bitmap);
      this->finish_send();
    }


    // handle these in your subclass
    virtual void on_greet(uint32_t number, uint64_t time) {
      fprintf(stderr, "Unimplemneted IPC handler \"on_greet\" on lumen server\n");
      return;
    }
    virtual void on_invalidate(uint32_t wid, ck::ref<gfx::rect> rects) {
      fprintf(stderr, "Unimplemneted IPC handler \"on_invalidate\" on lumen server\n");
      return;
    }
    virtual void on_open_window(int width, int height, ck::string window_name) {
      fprintf(stderr, "Unimplemneted IPC handler \"on_open_window\" on lumen server\n");
      return;
    }
    virtual void on_show_window(uint32_t wid) {
      fprintf(stderr, "Unimplemneted IPC handler \"on_show_window\" on lumen server\n");
      return;
    }
    virtual void on_hide_window(uint32_t wid) {
      fprintf(stderr, "Unimplemneted IPC handler \"on_hide_window\" on lumen server\n");
      return;
    }


   protected:
    // ^ck::ipc::impl::server_connection
    virtual inline void dispatch_received_message(void* data, size_t len) {
      ck::ipc::decoder __decoder(data, len);
      lumen::Message msg_type = (lumen::Message)ck::ipc::decode<uint32_t>(__decoder);
      switch (msg_type) {
        case lumen::Message::CLIENT_GREET: {
          uint32_t number;
          uint64_t time;
          ck::ipc::decode(__decoder, number);
          ck::ipc::decode(__decoder, time);
          on_greet(number, time);
          break;
        }
        case lumen::Message::CLIENT_INVALIDATE: {
          uint32_t wid;
          ck::ref<gfx::rect> rects;
          ck::ipc::decode(__decoder, wid);
          ck::ipc::decode(__decoder, rects);
          on_invalidate(wid, rects);
          break;
        }
        case lumen::Message::CLIENT_OPEN_WINDOW: {
          int width;
          int height;
          ck::string window_name;
          ck::ipc::decode(__decoder, width);
          ck::ipc::decode(__decoder, height);
          ck::ipc::decode(__decoder, window_name);
          on_open_window(width, height, window_name);
          break;
        }
        case lumen::Message::CLIENT_SHOW_WINDOW: {
          uint32_t wid;
          ck::ipc::decode(__decoder, wid);
          on_show_window(wid);
          break;
        }
        case lumen::Message::CLIENT_HIDE_WINDOW: {
          uint32_t wid;
          ck::ipc::decode(__decoder, wid);
          on_hide_window(wid);
          break;
        }
        default:
          panic("server: Unhandled message type %d", msg_type);
          break;
      }
    }
  };
}  // namespace lumen
