#pragma once

#include <ck/io.h>
#include <sys/socket.h>
#include <ck/string.h>

namespace ck {

  /**
   * A generic `sys/socket` implementation
   */
  class socket : public ck::file {

    int m_domain = 0;
    int m_type = 0;
    int m_proto = 0;
    bool m_connected = false;

   public:
		CK_OBJECT(ck::socket);
    socket(int domain, int type, int protocol = 0);
    virtual ~socket(void);

    bool connect(struct sockaddr *addr, size_t size);

    inline bool connected(void) { return m_connected; }

  };

	class localsocket : public ck::socket {
		public:
			CK_OBJECT(ck::localsocket);
			inline localsocket(void) : socket(AF_UNIX, SOCK_STREAM) {}
			inline virtual ~localsocket(void) {}

			bool connect(ck::string path);
	};

};  // namespace ck
