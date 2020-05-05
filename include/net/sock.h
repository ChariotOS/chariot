#pragma once

#include <lock.h>
#include <net/ipv4.h>
#include <net/socket.h>
#include <ptr.h>
#include <types.h>

// fwd decl
namespace fs {
struct inode;
}

namespace net {

struct sock;

/* Network protcol blocks that get attached to sockets */
struct proto {
  // required fields.
  int (*connect)(net::sock &sk, struct sockaddr *uaddr, int addr_len);
  int (*disconnect)(net::sock &sk, int flags);

  net::sock *(*accept)(net::sock &sk, int flags, int &err);

  int (*init)(net::sock &sk);
  void (*destroy)(net::sock &sk);  // called on ~net::sock()

  ssize_t (*send)(net::sock &sk, void *data, size_t len);
  ssize_t (*recv)(net::sock &sk, void *data, size_t len);

  // optional fields
  int (*ioctl)(net::sock &sk, int cmd, unsigned long arg);
};

/**
 * The representation of a network socket. Stored in fs::inode.sock when type
 * is T_SOCK
 */
struct sock {
	enum class role : uint8_t {
		none, accepting, listener, connected, connecting
	};

	net::sock::role role = role::none;

  int domain;	 // AF_*
  int type;	 // SOCK_*
  int protocol;	 // ignored in this kernel, but nice to have
  bool connected = false;
  size_t total_sent = 0;
  size_t total_recv = 0;

  sock(int domain, int type, int proto);
  virtual ~sock(void);

  template <typename T>
  T *&priv(void) {
    return (T *&)_private;
  }

  static net::sock *create(int domain, int type, int protocol, int &err);
  static fs::inode *createi(int domain, int type, int protocol, int &err);

  virtual int ioctl(int cmd, unsigned long arg);

  virtual int connect(struct sockaddr *uaddr, int addr_len);
  virtual int disconnect(int flags);
  virtual net::sock *accept(int flags, int &error);

  // implemented by the network layer (OSI)
  virtual ssize_t sendto(void *data, size_t len, int flags, const sockaddr *,
			 size_t);
  virtual ssize_t recvfrom(void *data, size_t len, int flags, const sockaddr *,
			   size_t);

  virtual int bind(const struct sockaddr *addr, size_t len);

 private:
  void *_private;
};

struct localsock : public net::sock {


  localsock(int type);
  virtual ~localsock(void);

  virtual int connect(struct sockaddr *uaddr, int addr_len);
  virtual int disconnect(int flags);
  virtual net::sock *accept(int flags, int &error);

  // implemented by the network layer (OSI)
  virtual ssize_t sendto(void *data, size_t len, int flags, const sockaddr *,
			 size_t);
  virtual ssize_t recvfrom(void *data, size_t len, int flags, const sockaddr *,
			   size_t);

  virtual int bind(const struct sockaddr *addr, size_t len);

  // intrusive linked list so we can store them all
  struct net::localsock *next, *prev;
};

struct ipv4sock : public net::sock {
 public:
  ipv4sock(int domain, int type, int proto);
  virtual ~ipv4sock(void);

  // send the buffer over ipv4
  ssize_t send_packet(void *pkt, size_t plen);

  // host ordered
  uint32_t peer_addr = 0;
  uint32_t local_addr = 0;
  uint16_t peer_port = 0;
  uint16_t local_port = 0;

  uint32_t bytes_received = 0;

  uint8_t ttl = 64;

  net::ipv4::route route;
  spinlock iplock;
};

struct udpsock : public net::ipv4sock {
 public:
  udpsock(int domain, int type, int proto);
  virtual ~udpsock(void);

  // implemented by the transport layer (OSI)
  virtual ssize_t sendto(void *data, size_t len, int flags, const sockaddr *,
			 size_t);
  virtual ssize_t recvfrom(void *data, size_t len, int flags, const sockaddr *,
			   size_t);

  int bind(const struct sockaddr *addr, size_t len);
};

}  // namespace net
