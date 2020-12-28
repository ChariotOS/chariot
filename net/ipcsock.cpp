#include <awaitfs.h>
#include <errno.h>
#include <fs/vfs.h>
#include <lock.h>
#include <net/sock.h>
#include <net/un.h>
#include <syscall.h>
#include <template_lib.h>
#include <util.h>

static rwlock all_ipcsocks_lock;
static struct net::ipcsock *all_ipcsocks = NULL;

net::ipcsock::ipcsock(int type) : net::sock(AF_CKIPC, type, 0) {
  all_ipcsocks_lock.write_lock();
  next = all_ipcsocks;
  prev = nullptr;
  all_ipcsocks = this;
  if (next != NULL) {
    next->prev = this;
  }
  all_ipcsocks_lock.write_unlock();
}

net::ipcsock::~ipcsock(void) {
  all_ipcsocks_lock.write_lock();
  if (all_ipcsocks == this) {
    all_ipcsocks = next;
  }

  if (next) next->prev = prev;
  if (prev) prev->next = next;

  // if this socket is bound, release it
  if (bindpoint != NULL) {
    // we don't need to
    bindpoint->bound_socket = NULL;
    fs::inode::release(bindpoint);
  }
  all_ipcsocks_lock.write_unlock();
}


net::sock *net::ipcsock::accept(struct sockaddr *uaddr, int addr_len,
                                int &err) {
  // wait on a client
  auto *client = pending_connections.recv();
  return client;
}

int net::ipcsock::connect(struct sockaddr *addr, int len) {
  if (len != sizeof(struct sockaddr_un)) {
    return -EINVAL;
  }

  auto un = (struct sockaddr_un *)addr;
  string path;
  for (int i = 0; i < UNIX_PATH_MAX; i++) {
    char c = un->sun_path[i];
    if (c == '\0') break;
    path.push(c);
  }

  auto in = vfs::open(path, O_RDWR);
  if (in == NULL) return -ENOENT;

  if (in->bound_socket == NULL) {
    return -ENOENT;
  }

  peer = (struct ipcsock *)net::sock::acquire(*in->bound_socket);

  // send, and wait. This will always succeed if we are here.
  this->peer->pending_connections.send(this, true);

  // assume the above succeeded
  return 0;
}


int net::ipcsock::disconnect(int flags) {
  auto &state = (flags & PFLAGS_CLIENT) ? for_server : for_client;

  state.lock.lock();
  state.closed = true;
  state.wq.wake_up_all();
  state.lock.unlock();

  return 0;
}


static spinlock loglock;
void named_hexdump(const char *msg, void *buf, size_t sz) {
  loglock.lock();
  printk(
      " %s "
      "===================================================================\n",
      msg);
  hexdump(buf, sz, true);
  /*
printk(
" !!!! "
"===================================================================\n");
                  */
  loglock.unlock();
}

// #define IPCSOCK_DEBUG


ssize_t net::ipcsock::sendto(fs::file &fd, void *data, size_t len, int flags,
                             const sockaddr *, size_t) {
  auto &state = (fd.pflags & PFLAGS_SERVER) ? for_client : for_server;

  scoped_lock l(state.lock);
  if (state.closed) {
    return 0;
  }

  state.msgs.append(ipcmsg(data, len));
  state.wq.wake_up();


  return len;
}



ssize_t net::ipcsock::recvfrom(fs::file &fd, void *data, size_t len, int flags,
                               const sockaddr *, size_t) {
  auto &state = (fd.pflags & PFLAGS_SERVER) ? for_server : for_client;
  bool block = (flags & MSG_DONTWAIT) == 0;


  if (!block) {
    scoped_lock l(state.lock);

    if (state.msgs.is_empty()) {
      if (state.closed) return 0;
      return -EAGAIN;
    }
  }


  while (1) {
    {
      scoped_lock l(state.lock);

      if (!state.msgs.is_empty()) {
        auto &front = state.msgs.first();
        if (flags & MSG_IPC_QUERY) {
          return front.data.size();
        }

        // the buffer needs to be big enough
        if (front.data.size() > len) return -EMSGSIZE;

        memcpy(data, front.data.data(), front.data.size());
        size_t nread = front.data.size();
        state.msgs.take_first();

        return nread;
      }
      if (state.closed) {
        return 0;
      }
    }


    if (!block) return -EAGAIN;

    if (state.wq.wait().interrupted()) {
      printk("WAIT!\n");
      return -EINTR;
    }
  }
}



int net::ipcsock::bind(const struct sockaddr *addr, size_t len) {
  if (len != sizeof(struct sockaddr_un)) return -EINVAL;
  if (bindpoint != NULL) return -EINVAL;

  auto un = (struct sockaddr_un *)addr;

  string path;
  for (int i = 0; i < UNIX_PATH_MAX; i++) {
    char c = un->sun_path[i];
    if (c == '\0') break;
    path.push(c);
  }

  auto in = vfs::open(path, O_RDWR);
  if (in == NULL) return -ENOENT;
  // has someone already bound to this file?
  if (in->bound_socket != NULL) return -EADDRINUSE;

  // acquire the file and set the bound_socket to this
  bindpoint = fs::inode::acquire(in);
  bindpoint->bound_socket = net::sock::acquire(*this);
  return 0;
}


int net::ipcsock::poll(fs::file &f, int events, poll_table &pt) {
  int res = 0;


  if (f.pflags & PFLAGS_CLIENT) {
    scoped_lock l(for_client.lock);

		pt.wait(for_client.wq, AWAITFS_READ);
    if (!for_client.msgs.is_empty()) {
      res |= AWAITFS_READ;
    }

    if (for_client.closed) {
      res |= AWAITFS_READ;
    }
  }


  if (f.pflags & PFLAGS_SERVER) {
    scoped_lock l(for_server.lock);

		pt.wait(for_server.wq, AWAITFS_READ);
    if (!for_server.msgs.is_empty()) {
      res |= AWAITFS_READ;
    }
    if (for_server.closed) {
      res |= AWAITFS_READ;
    }
  }

  // pending connections are considered a read
  res |= pending_connections.poll(pt) & AWAITFS_READ;
  return (res & events);
}
