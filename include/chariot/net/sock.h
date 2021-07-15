#pragma once

#include <chan.h>
#include <fifo_buf.h>
#include <lock.h>
#include <net/ipv4.h>
#include <net/socket.h>
#include <ptr.h>
#include <sem.h>
#include <single_list.h>
#include <types.h>


// fwd decl
namespace fs {
  struct inode;
  class file;
}  // namespace fs

#define PFLAGS_SERVER (0x1)
#define PFLAGS_CLIENT (0x2)

namespace net {

  struct sock;

  /* Network protcol blocks that get attached to sockets */
  struct proto {
    // required fields.
    int (*connect)(net::sock &sk, struct sockaddr *uaddr, int addr_len);
    int (*disconnect)(net::sock &sk, int flags);

    net::sock *(*accept)(net::sock &sk, struct sockaddr *uaddr, int addr_len, int &err);

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
    enum class role : uint8_t { none, accepting, listener, connected, connecting };

    net::sock::role role = role::none;

    int domain;    // AF_*
    int type;      // SOCK_*
    int protocol;  // ignored in this kernel, but nice to have
    bool connected = false;
    size_t total_sent = 0;
    size_t total_recv = 0;

    // the inode that contains this socket
    struct fs::inode *ino = NULL;


    sock(int domain, int type, int proto);
    virtual ~sock(void);

    inline virtual int poll(fs::file &f, int events, poll_table &pt) { return 0; }

    template <typename T>
    T *&priv(void) {
      return (T *&)_private;
    }

    static net::sock *create(int domain, int type, int protocol, int &err);
    static fs::inode *createi(int domain, int type, int protocol, int &err);

    static net::sock *acquire(net::sock &);
    static void release(net::sock *&);

    virtual int ioctl(int cmd, unsigned long arg);

    virtual int connect(struct sockaddr *uaddr, int addr_len);
    virtual net::sock *accept(struct sockaddr *uaddr, int addr_len, int &err);
    virtual int disconnect(int flags);

    // implemented by the network layer (OSI)
    virtual ssize_t sendto(fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t);
    virtual ssize_t recvfrom(
        fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t);

    virtual int bind(const struct sockaddr *addr, size_t len);

   private:
    spinlock owners_lock;
    int owners = 0;
    void *_private;
  };

  struct localsock : public net::sock {
    localsock(int type);
    virtual ~localsock(void);

    virtual int connect(struct sockaddr *uaddr, int addr_len);
    virtual int disconnect(int flags);
    virtual net::sock *accept(struct sockaddr *uaddr, int addr_len, int &err);

    // implemented by the network layer (OSI)
    virtual ssize_t sendto(fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t);
    virtual ssize_t recvfrom(
        fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t);

    virtual int bind(const struct sockaddr *addr, size_t len);

    virtual int poll(fs::file &f, int events, poll_table &pt);


    void dump_stats(void);

    // the inode this (server) socket is bound to
    fs::inode *bindpoint = nullptr;

    struct localsock *peer;

    fifo_buf for_server;  // client writes, server reads
    fifo_buf for_client;  // server writes, client reads

    bool is_server = false;

    chan<localsock *> pending_connections;

    // intrusive linked list so we can store them all
    struct net::localsock *next, *prev;
  };



  // ipc messages cannot be partially consumed,
  // they must be entirely read at once
  struct ipcmsg {
    ck::vec<uint8_t> data;

    inline ipcmsg(void *buf, size_t sz) {
      data.ensure_capacity(sz);
      data.push((uint8_t *)buf, sz);
    }
  };

  struct ipcsock : public net::sock {
    ipcsock(int type);
    virtual ~ipcsock(void);

    virtual int connect(struct sockaddr *uaddr, int addr_len);
    virtual int disconnect(int flags);
    virtual net::sock *accept(struct sockaddr *uaddr, int addr_len, int &err);

    // implemented by the network layer (OSI)
    virtual ssize_t sendto(fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t);
    virtual ssize_t recvfrom(
        fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t);

    virtual int bind(const struct sockaddr *addr, size_t len);

    virtual int poll(fs::file &f, int events, poll_table &pt);


    // the inode this (server) socket is bound to
    fs::inode *bindpoint = nullptr;

    struct ipcsock *peer;


    struct {
      ck::single_list<ipcmsg> msgs;
      struct wait_queue wq;
      spinlock lock;
      bool closed = false;
    } for_server, for_client;


    chan<ipcsock *> pending_connections;
    // intrusive linked list so we can store them all
    struct net::ipcsock *next, *prev;
  };



#ifdef CONFIG_LWIP
  /* A generic LWIP socket binding. This ties into the socket api. The implementation is in
   * /net/lwip/api/sockets.cpp */
  struct ip4sock : public net::sock {
   private:
    /* The socket binding */
    int sock;
    spinlock lock;

   public:
    ip4sock(int type);
    virtual ~ip4sock(void);


    virtual int connect(struct sockaddr *uaddr, int addr_len);
    virtual int disconnect(int flags);
    virtual net::sock *accept(struct sockaddr *uaddr, int addr_len, int &err);

    // implemented by the network layer (OSI)
    virtual ssize_t sendto(fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t);
    virtual ssize_t recvfrom(
        fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t);

    virtual int bind(const struct sockaddr *addr, size_t len);

    virtual int poll(fs::file &f, int events, poll_table &pt);

    int translate_errno(int);
  };
#endif

}  // namespace net
