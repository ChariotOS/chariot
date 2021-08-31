#pragma once

#include <ck/string.h>
#include <ck/io.h>
#include <ck/socket.h>
#include <ck/ptr.h>
#include <pthread.h>
#include <ck/pair.h>

#define IPC_USE_SHM


namespace ck {

  namespace ipc {


    // using 32bits means the ``header'' of each sync message is 8 bytes total.
    // This size is *ONLY* worse than a 64bit nonce if you send 4,294,967,295
    // messages without receiving them on the peer connection. This would be
    // insane, and I'm not sure the rest of the system would even be able to
    // handle it. Seeing as the overhead of each message in the kernel is AT
    // LEAST 64 bytes... you need alot of ram
    using nonce_t = uint32_t;

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
        if (head >= sz) {
          printf("head: %d, sz: %d\n", head, sz);
        }
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

    template <typename T>
    inline bool decode(ck::ipc::decoder& d, ck::vec<T>& v) {
      auto sz = d.read<uint32_t>();
      for (int i = 0; i < sz; i++) {
        v.push(d.read<T>());
      }
      return true;
    }

#undef SIMPLE_DECODER




    namespace impl {

      class socket_connection : public ck::refcounted<socket_connection> {
       public:
        socket_connection(ck::ref<ck::ipcsocket> s, const char* ns, bool is_server);
        virtual ~socket_connection(void) { printf("ipc connection killed!\n"); }

        // a closed socket connection is just one without a socket :^)
        void close(void) { m_sock = nullptr; }
        bool closed(void) { return !m_sock; }

        ck::func<void()> on_close;

       protected:
        // the encoder writes directly to the send_queue, finish-send just hits the doorbell
        ipc::encoder begin_send(void);
        void finish_send();
        void dispatch(void);
        void handshake(void);
        void drain_messages(void);
        virtual void dispatch_received_message(void* data, size_t len) = 0;
        // wait for a message where the first uint32_t is the needle.
        // Return a copy of the message when found, blocking forever
        // if no message is found. Any other messages are placed
        // in the stored_messages vector that needs to be dispatched
        // via dispatch_stored
        ck::pair<void*, size_t> sync_wait(uint32_t msg_type, ck::ipc::nonce_t nonce);

        struct stored_message {
          void* data;
          size_t len;
        };
        ck::vec<struct stored_message> stored_messages;



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


        ck::ref<ck::ipcsocket> m_sock;
        const char* ns;
        bool is_server;

        // each side gets 32kb of shared memory
        size_t total_shared_msg_queue_size = 64 * 4096;
        void* shared_msg_queue = NULL;

        // server recvs from position 0, sends to position 1
        // client recvs from position 1, sends to position 0
        struct shared_msg_queue* recv_queue = NULL;
        struct shared_msg_queue* send_queue = NULL;


#ifdef IPC_USE_SHM
        ck::string shm_name;
#else
        void* msg_buffer = NULL;
        size_t msg_size = 0;
#endif

        nonce_t get_nonce(void) {
          auto n = next_nonce++;
          return n;
        }
        nonce_t next_nonce = 0;
      };

    }  // namespace impl

    template <typename Connection>
    class server {
     public:
      server(void) = default;



      // start the server listening on a given path
      void listen(ck::string listen_path) {
        assert(!m_server.listening());
        m_server.listen(listen_path, [this] {
          ck::ipcsocket* sock = m_server.accept();

          this->handle_connection(ck::make_unique<Connection>(sock));
        });
        m_listen_path = listen_path;
      }

     private:
      void handle_connection(ck::unique_ptr<Connection>&& s) {
        printf("SERVER GOT A CONNECTION!\n");
        Connection* sp = s.get();
        sp->on_close = [this, sp] {
          for (int i = 0; i < this->m_connections.size(); i++) {
            if (this->m_connections[i].get() == sp) {
              this->m_connections.remove(i);
              break;
            }
          }
        };

        m_connections.push(move(s));
      }

      ck::vec<ck::unique_ptr<Connection>> m_connections;
      ck::string m_listen_path;
      ck::ipcsocket m_server;
    };



  }  // namespace ipc
}  // namespace ck