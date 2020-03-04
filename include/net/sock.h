#pragma once

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

// register a protocol with an SOCK_*
void register_proto(net::proto &, int typ);
net::proto *lookup_proto(int type);

/**
 * The representation of a network socket. Stored in fs::inode.sock when type
 * is T_SOCK
 */
struct sock {
  uint16_t domain;  // AF_*
  int type;         // SOCK_*
  int protocol;     // ignored in this kernel, but nice to have
  bool connected = false;

  // a reference to the socket's protocol block
  struct proto &prot;

  sock(uint16_t d, int type, struct proto &p);
  ~sock(void);

  template <typename T>
  T *&priv(void) {
    return (T *&)_private;
  }

  static net::sock *create(int domain, int type, int protocol, int &err);
  static fs::inode *createi(int domain, int type, int protocol, int&err);

 private:
  void *_private;
};

}  // namespace net
