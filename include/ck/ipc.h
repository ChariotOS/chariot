#pragma once

#include <ck/string.h>
#include <ck/io.h>
#include <ck/socket.h>
#include <ck/ptr.h>
#include <pthread.h>

namespace ck {

  namespace ipc {

    class encoder;
    class decoder;


    template <typename T>
    bool encode(ck::ipc::encoder&, T&) {
      panic("base ck::ipc::encode instantiated\n");
      return false;
    }




    class encoder {
      //   encoder(void* buf, size_t sz) : buf(buf), sz(sz) {}
      uint8_t* m_buf;
      size_t& m_size;


     public:
      inline encoder(void* buf, size_t& size_out) : m_buf((uint8_t*)buf), m_size(size_out) {}
      template <typename T>
      encoder& operator<<(T& v) {
        ck::ipc::encode(*this, v);
        return *this;
      }

      template <typename T>
      encoder& operator<<(T v) {
        ck::ipc::encode(*this, v);
        return *this;
      }

      template <typename T>
      T& next(void) {
        auto cursz = m_size;

        m_size += sizeof(T);
        return *(T*)&m_buf[cursz];
      }


      void write(const void* buf, size_t sz) {
        memcpy(m_buf + m_size, buf, sz);
        m_size += sz;
      }

      void* data(void) { return (void*)m_buf; }
      size_t size(void) { return m_size; }
    };




#define SIMPLE_ENCODER(T)                         \
  template <>                                     \
  inline bool encode(ck::ipc::encoder& e, T& v) { \
    e.next<T>() = v;                              \
    return true;                                  \
  }

    SIMPLE_ENCODER(uint8_t);
    SIMPLE_ENCODER(int8_t);

    SIMPLE_ENCODER(uint16_t);
    SIMPLE_ENCODER(int16_t);

    SIMPLE_ENCODER(uint32_t);
    SIMPLE_ENCODER(int32_t);

    SIMPLE_ENCODER(uint64_t);
    SIMPLE_ENCODER(int64_t);

    SIMPLE_ENCODER(size_t);

    // no string or vector will be larger than 4gb, so we just use uint32_t to save 4 bytes :)

    template <>
    inline bool encode(ck::ipc::encoder& e, ck::string& v) {
      e << (uint32_t)v.size();
      e.write(v.get(), v.size());
      return true;
    }


    template <typename T>
    inline bool encode(ck::ipc::encoder& e, ck::vec<T>& v) {
      e << (uint32_t)v.size();
      for (auto& c : v) {
        ck::ipc::encode(e, c);
      }
      return true;
    }


#undef SIMPLE_ENCODER



    class decoder {
      void* buf;
      size_t sz;
      off_t head = 0;

     public:
      decoder(void* buf, size_t sz) : buf(buf), sz(sz) {}

      template <typename T>
      T& next(void) {
        assert(head < sz);
        auto curhead = head;
        head += sizeof(T);

        return *(T*)((unsigned char*)buf + curhead);
      }

      template <typename T>
      T read() {
        return next<T>();
      }
    };


    // TODO:
    template <typename T>
    bool decode(ck::ipc::decoder&, T&) {
      panic("base ck::ipc::decode instantiated\n");
      return false;
    }

    template <typename T>
    T decode(ck::ipc::decoder& d) {
      T val;
      ck::ipc::decode(d, val);
      return val;
    }


#define SIMPLE_DECODER(T)                         \
  template <>                                     \
  inline bool decode(ck::ipc::decoder& d, T& v) { \
    v = d.next<T>();                              \
    return true;                                  \
  }

    SIMPLE_DECODER(uint8_t);
    SIMPLE_DECODER(int8_t);

    SIMPLE_DECODER(uint16_t);
    SIMPLE_DECODER(int16_t);

    SIMPLE_DECODER(uint32_t);
    SIMPLE_DECODER(int32_t);

    SIMPLE_DECODER(uint64_t);
    SIMPLE_DECODER(int64_t);

    SIMPLE_DECODER(size_t);

    // no string or vector will be larger than 4gb, so we just use uint32_t to save 4 bytes :)

    template <>
    inline bool decode(ck::ipc::decoder& d, ck::string& v) {
      auto sz = d.read<uint32_t>();
      for (int i = 0; i < sz; i++) {
        v.push(d.read<uint8_t>());
      }
      return true;
    }

#undef SIMPLE_DECODER




    namespace impl {

      class socket_connection : public ck::refcounted<socket_connection> {
       public:
        socket_connection(ck::ref<ck::ipcsocket> s, const char* ns, bool is_server);
        virtual ~socket_connection(void) { printf("ipc connection killed!\n"); }

       protected:
        // the encoder writes directly to the send_queue, finish-send just hits the doorbell
        ipc::encoder begin_send(void);
        void finish_send();




        struct shared_msg_data {
          size_t size;
          uint8_t data[];
        };

        struct shared_msg_queue {
          // we can use a mutex here because we know it's only an integer, nothing fancy
          pthread_mutex_t lock;
          ck::atom<int> count;
          struct shared_msg_data data[];
        };




        struct msg_handshake {
          size_t total_shared_msg_queue_size;
          char shm_name[32];
        };


        template <typename T>
        T recv_sync(void) {
          T out;
          m_sock->recv((void*)&out, sizeof(T), 0);
          return out;
        }

        void handshake(void);
        void drain_messages(void);
        virtual void dispatch_received_message(void* data, size_t len) = 0;
        ck::ref<ck::ipcsocket> m_sock;
        const char* ns;
        bool is_server;

        // each side gets 32kb of shared memory
        size_t total_shared_msg_queue_size = 64 * 4096;
        void* shared_msg_queue = NULL;

        // server recvs from position 0, sends to position 1
        // client recvs from position 1, sends to position 0
        struct shared_msg_queue* recv_queue;
        struct shared_msg_queue* send_queue;


        ck::string shm_name;
      };

    }  // namespace impl

  }  // namespace ipc
}  // namespace ck