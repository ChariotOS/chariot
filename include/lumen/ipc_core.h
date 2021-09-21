/**
 *  Generated by ipcc.py. Any changes will be overwritten
 *
 * Namespace: lumen
 *
 * Client messages:
 *
 *   create_window (
 *   )
 *
 *   set_window_name (
 *     uint32_t wid
 *     ck::string name
 *   )
 *
 *
 * Server messages:
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
    CLIENT_CREATE_WINDOW,
    CLIENT_CREATE_WINDOW_RESPONSE,
    CLIENT_SET_WINDOW_NAME,
    CLIENT_SET_WINDOW_NAME_RESPONSE
  };
  // return value for create_window sent from client
  struct client_create_window_response {
    uint32_t wid;
  };
  // return value for set_window_name sent from client
  struct client_set_window_name_response {
    int result;
  };
};



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
      client_connection_stub(ck::ref<ck::ipcsocket> s) : ck::ipc::impl::socket_connection(s, "lumen", false) {}
      virtual ~client_connection_stub(void) {}

      // methods to send messages
      inline ck::option<struct client_create_window_response> create_window() {
        if (closed()) return None;
        ck::ipc::encoder __e = begin_send();
        __e << (uint32_t)lumen::Message::CLIENT_CREATE_WINDOW;
        ck::ipc::nonce_t __nonce = get_nonce();
        __e << (ck::ipc::nonce_t)__nonce;
        this->finish_send();
        // wait for the return value from the server :^)
        struct client_create_window_response res;
        auto [data, len] = sync_wait((uint32_t)lumen::Message::CLIENT_CREATE_WINDOW_RESPONSE, __nonce);
        if (data == NULL && len == 0) return None;
        ck::ipc::decoder __decoder(data, len);
        auto __response_type = (lumen::Message)ck::ipc::decode<uint32_t>(__decoder);
        auto __response_nonce = ck::ipc::decode<ck::ipc::nonce_t>(__decoder);
        assert(__response_type == lumen::Message::CLIENT_CREATE_WINDOW_RESPONSE);
        assert(__response_nonce == __nonce);
        ck::ipc::decode(__decoder, res.wid);
        return res;
      }

      inline ck::option<struct client_set_window_name_response> set_window_name(uint32_t wid, ck::string name) {
        if (closed()) return None;
        ck::ipc::encoder __e = begin_send();
        __e << (uint32_t)lumen::Message::CLIENT_SET_WINDOW_NAME;
        ck::ipc::nonce_t __nonce = get_nonce();
        __e << (ck::ipc::nonce_t)__nonce;
        ck::ipc::encode(__e, wid);
        ck::ipc::encode(__e, name);
        this->finish_send();
        // wait for the return value from the server :^)
        struct client_set_window_name_response res;
        auto [data, len] = sync_wait((uint32_t)lumen::Message::CLIENT_SET_WINDOW_NAME_RESPONSE, __nonce);
        if (data == NULL && len == 0) return None;
        ck::ipc::decoder __decoder(data, len);
        auto __response_type = (lumen::Message)ck::ipc::decode<uint32_t>(__decoder);
        auto __response_nonce = ck::ipc::decode<ck::ipc::nonce_t>(__decoder);
        assert(__response_type == lumen::Message::CLIENT_SET_WINDOW_NAME_RESPONSE);
        assert(__response_nonce == __nonce);
        ck::ipc::decode(__decoder, res.result);
        return res;
      }


      // handle these in your subclass


    protected:
      // ^ck::ipc::impl::client_connection
      virtual inline void dispatch_received_message(void *data, size_t len) {
        ck::ipc::decoder __decoder(data, len);
        lumen::Message msg_type = (lumen::Message)ck::ipc::decode<uint32_t>(__decoder);
        switch (msg_type) {
          default:
            panic("client: Unhandled message type %d", msg_type);
            break;
        }
      }
  };
}


namespace lumen {
  class server_connection_stub : public ck::ipc::impl::socket_connection {
    public:
      server_connection_stub(ck::ref<ck::ipcsocket> s) : ck::ipc::impl::socket_connection(s, "lumen", true) {}
      virtual ~server_connection_stub(void) {}

      // methods to send messages

      // handle these in your subclass
      virtual ck::option<struct client_create_window_response> on_create_window() {
        fprintf(stderr, "Unimplemneted IPC handler \"on_create_window\" on lumen server\n");
        return { };
      }
      virtual ck::option<struct client_set_window_name_response> on_set_window_name(uint32_t wid, ck::string name) {
        fprintf(stderr, "Unimplemneted IPC handler \"on_set_window_name\" on lumen server\n");
        return { };
      }


    protected:
      // ^ck::ipc::impl::server_connection
      virtual inline void dispatch_received_message(void *data, size_t len) {
        ck::ipc::decoder __decoder(data, len);
        lumen::Message msg_type = (lumen::Message)ck::ipc::decode<uint32_t>(__decoder);
        switch (msg_type) {
          case lumen::Message::CLIENT_CREATE_WINDOW: {
            auto __nonce = ck::ipc::decode<ck::ipc::nonce_t>(__decoder);
            auto __res = on_create_window();
            auto __out = __res.unwrap();
            // now to send the response back
            ck::ipc::encoder __e = begin_send();
            __e << (uint32_t)lumen::Message::CLIENT_CREATE_WINDOW_RESPONSE;
            __e << (ck::ipc::nonce_t)__nonce;
            ck::ipc::encode(__e, __out.wid);
            this->finish_send();
            break;
          }
          case lumen::Message::CLIENT_SET_WINDOW_NAME: {
            auto __nonce = ck::ipc::decode<ck::ipc::nonce_t>(__decoder);
            uint32_t wid;
            ck::string name;
            ck::ipc::decode(__decoder, wid);
            ck::ipc::decode(__decoder, name);
            auto __res = on_set_window_name(wid, name);
            auto __out = __res.unwrap();
            // now to send the response back
            ck::ipc::encoder __e = begin_send();
            __e << (uint32_t)lumen::Message::CLIENT_SET_WINDOW_NAME_RESPONSE;
            __e << (ck::ipc::nonce_t)__nonce;
            ck::ipc::encode(__e, __out.result);
            this->finish_send();
            break;
          }
          default:
            panic("server: Unhandled message type %d", msg_type);
            break;
        }
      }
  };
}
