#pragma once

#include <sys/un.h>
#include <ck/io.h>
#include <ck/string.h>
#include <sys/socket.h>
#include <ck/fsnotifier.h>


namespace ck {

  /**
   * A generic `sys/socket` implementation
   */
  class socket : public ck::file {
   protected:
    int m_domain = 0;
    int m_type = 0;
    int m_proto = 0;
    bool m_connected = false;
    bool m_listening = false;

		ck::fsnotifier notifier;

		socket(int fd, int domain, int type, int protocol = 0);
   public:
    socket(int domain, int type, int protocol = 0);
    virtual ~socket(void);

    ssize_t send(void *buf, size_t sz, int flags);
    ssize_t recv(void *buf, size_t sz, int flags);

    bool connect(struct sockaddr *addr, size_t size);

    inline bool connected(void) { return m_connected; }
    inline bool listening(void) { return m_listening; }



    ck::func<void()> on_read;
    ck::func<void()> on_write;

    CK_OBJECT(ck::socket);
  };

  class localsocket : public ck::socket {

    inline localsocket(int fd) : socket(fd, AF_UNIX, SOCK_STREAM, 0) {}
   public:
    inline localsocket(void) : socket(AF_UNIX, SOCK_STREAM) {}
    inline virtual ~localsocket(void) {}

    bool connect(ck::string path);
    int bind(ck::string path);

		ref<ck::localsocket> accept(void);

    CK_OBJECT(ck::localsocket);

	 private:
  	struct sockaddr_un addr;
  };

};  // namespace ck
