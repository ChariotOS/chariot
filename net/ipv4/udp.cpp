#include <errno.h>
#include <module.h>
#include <net/ipv4.h>
#include <net/sock.h>
#include <util.h>

// every udp socket uses this as its private data
struct udp_blk {};

// nice macro to make accessing the udp block of a socket cleaner
#define ublk(sk) (sk.priv<struct udp_blk>())


static net::sock *udp_accept(net::sock &sk, int flags, int &error) {
  error = 0;
  return NULL;
}

static int udp_init(net::sock &sk) {
  ublk(sk) = new udp_blk;
  return 0;
}

static void udp_destroy(net::sock &sk) {
  // TODO: teardown internal state
  // printk("udp_destroy\n");

  // make sure to disconnect
  if (sk.connected) {
    sk.prot.disconnect(sk, 0 /* TODO: flags? */);
  }
  delete ublk(sk);
}

static ssize_t udp_send(net::sock &sk, void *v, size_t s) {
  if (!sk.connected) return -1;
  printk("udp send:\n");
  hexdump(v, s, true);
  return -1;
}

static ssize_t udp_recv(net::sock &sk, void *, size_t) {
  if (!sk.connected) return -1;
  return -ENOTIMPL;
}

// the UDP control function block
net::proto udp_proto{
    .connect = net::ipv4::datagram_connect,
    .disconnect = net::ipv4::datagram_disconnect,

    .accept = udp_accept,
    .init = udp_init,
    .destroy = udp_destroy,

    .send = udp_send,
    .recv = udp_recv,
};

static void udp_mod_init(void) { net::register_proto(udp_proto, SOCK_DGRAM); }

module_init("udp", udp_mod_init);
