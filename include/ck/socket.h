#pragma once

// #ifdef __cplusplus

#include <ck/fsnotifier.h>
#include <ck/io.h>
#include <ck/map.h>
#include <ck/string.h>
#include <ck/vec.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

namespace ck {

  /**
   * A generic `sys/socket` implementation
   */
  class Socket : public ck::File {
   protected:
    int m_domain = 0;
    int m_type = 0;
    int m_proto = 0;
    bool m_connected = false;
    bool m_listening = false;

    Socket(int fd, int domain, int type, int protocol = 0);

   public:
    Socket(int domain, int type, int protocol = 0);
    virtual ~Socket(void);

    ssize_t send(void *buf, size_t sz, int flags);
    ssize_t recv(void *buf, size_t sz, int flags);

    async(ssize_t) async_recv(void *buf, size_t sz, int flags);


    bool connect(struct sockaddr *addr, size_t size);

    inline bool connected(void) { return m_connected; }
    inline bool listening(void) { return m_listening; }

    void set_connected(bool to = true) { m_connected = to; }


    CK_OBJECT(ck::Socket);
  };

  class LocalSocket : public ck::Socket {
    inline LocalSocket(int fd) : Socket(fd, AF_UNIX, SOCK_STREAM, 0) { m_connected = true; }

   public:
    inline LocalSocket(void) : Socket(AF_UNIX, SOCK_STREAM) {}
    inline virtual ~LocalSocket(void) {}

    bool connect(ck::string path);
    int listen(ck::string path, ck::func<void()> cb);

    ck::LocalSocket *accept(void);

    CK_OBJECT(ck::LocalSocket);

   private:
    struct sockaddr_un addr;
  };




  class IPCSocket : public ck::Socket {
    inline IPCSocket(int fd) : Socket(fd, AF_CKIPC, SOCK_DGRAM, 0) {}

   public:
    inline IPCSocket(void) : Socket(AF_CKIPC, SOCK_DGRAM) {}
    inline virtual ~IPCSocket(void) {}

    bool connect(ck::string path);
    int listen(ck::string path, ck::func<void()> cb);

    ck::IPCSocket *accept(void);

    CK_OBJECT(ck::LocalSocket);

    /*
     */
    template <typename T>
    auto drain(void) {
      bool failed = false;
      ck::vec<T *> msgs;

      while (1) {
        uint8_t buf;
        int sz = recv(&buf, 1, MSG_IPC_QUERY | MSG_DONTWAIT);

        if (sz < 0 || eof()) {
          if (errno == EAGAIN) break;
          failed = true;
          break;
        }

        char *buffer = (char *)malloc(sz);
        int nread = recv(buffer, sz, 0);
        if (nread != sz || eof()) {
          free(buffer);
          failed = true;
          break;
        }
        auto *msg = (T *)buffer;
        msgs.push(msg);
      }
      if (failed) set_eof(true);
      return msgs;
    }

   private:
    struct sockaddr_un addr;
  };

};  // namespace ck

// #endif
