#include <awaitfs.h>
#include <errno.h>
#include <fs/vfs.h>
#include <lock.h>
#include <net/sock.h>
#include <net/un.h>
#include <syscall.h>
#include <util.h>


net::LocalSocket::LocalSocket(int type) : net::Socket(AF_LOCAL, type, 0) {}

net::LocalSocket::~LocalSocket(void) {
  // if this socket is bound, release it
  if (bindpoint != nullptr) {
    // we don't need to
    bindpoint->bound_socket = nullptr;
  }
}


ck::ref<net::Socket> net::LocalSocket::accept(struct sockaddr *uaddr, int addr_len, int &err) {
  // wait on a client
  auto client = pending_connections.recv();
  return client;
}

int net::LocalSocket::connect(struct sockaddr *addr, int len) {
  if (len != sizeof(struct sockaddr_un)) {
    return -EINVAL;
  }

  auto un = (struct sockaddr_un *)addr;
  ck::string path;
  for (int i = 0; i < UNIX_PATH_MAX; i++) {
    char c = un->sun_path[i];
    if (c == '\0') break;
    path.push(c);
  }

  auto in = vfs::open(path, O_RDWR);
  if (in == nullptr) return -ENOENT;

  if (in->bound_socket == nullptr) {
    return -ENOENT;
  }

  peer = in->bound_socket;

  // send, and wait. This will always succeed if we are here.
  this->peer->pending_connections.send(this, true);

  // assume the above succeeded
  return 0;
}


int net::LocalSocket::disconnect(int flags) {
  if (flags & PFLAGS_CLIENT) {
    // printf("client disconnect\n");
    for_server.close();
  }
  if (flags & PFLAGS_SERVER) {
    // printf("server disconnect\n");
    for_client.close();
  }
  return 0;
}

#define LOCALSOCK_DUMP_STATS

#ifdef LOCALSOCK_DUMP_STATS
static spinlock localsocket_stat_lock;
#endif

void net::LocalSocket::dump_stats(void) {
#ifdef LOCALSOCK_DUMP_STATS
  scoped_lock l(localsocket_stat_lock);
  size_t server_avail, server_unread;
  size_t client_avail, client_unread;

  for_server.stats(server_avail, server_unread);
  for_client.stats(client_avail, client_unread);
  printf(KERN_DEBUG "stats: server: (a: %-4zu, u: %-4zu) client: (a: %-4zu, u: %-4zu)\n", server_avail, server_unread, client_avail,
      client_unread);
#endif
}




ssize_t net::LocalSocket::sendto(fs::File &fd, void *data, size_t len, int flags, const sockaddr *, size_t) {
  auto &buf = (fd.pflags & PFLAGS_SERVER) ? for_client : for_server;
  auto n = buf.write(data, len, (flags & MSG_DONTWAIT) == 0);
  dump_stats();
  return n;
}
ssize_t net::LocalSocket::recvfrom(fs::File &fd, void *data, size_t len, int flags, const sockaddr *, size_t) {
  auto &buf = (fd.pflags & PFLAGS_SERVER) ? for_server : for_client;
  auto n = buf.read(data, len, (flags & MSG_DONTWAIT) == 0);
  dump_stats();
  return n;
}

int net::LocalSocket::bind(const struct sockaddr *addr, size_t len) {
  if (len != sizeof(struct sockaddr_un)) return -EINVAL;
  if (bindpoint != nullptr) return -EINVAL;

  auto un = (struct sockaddr_un *)addr;

  ck::string path;
  for (int i = 0; i < UNIX_PATH_MAX; i++) {
    char c = un->sun_path[i];
    if (c == '\0') break;
    path.push(c);
  }

  auto in = vfs::open(path, O_RDWR);
  if (in == nullptr) return -ENOENT;
  // has someone already bound to this file?
  if (in->bound_socket != nullptr) return -EADDRINUSE;

  // acquire the file and set the bound_socket to this
  bindpoint = in;
  bindpoint->bound_socket = this;
  return 0;
}


int net::LocalSocket::poll(fs::File &f, int events, poll_table &pt) {
  int res = 0;

  if (f.pflags & PFLAGS_CLIENT) {
    res |= for_client.poll(pt) & (AWAITFS_READ);
  } else if (f.pflags & PFLAGS_SERVER) {
    // A pending connection is considered an AWAITFS_READ event
    res |= for_server.poll(pt) & (AWAITFS_READ);
  }

  res |= pending_connections.poll(pt) & AWAITFS_READ;

  return res & events & AWAITFS_READ;
}
