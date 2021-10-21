#include <awaitfs.h>
#include <errno.h>
#include <fs/vfs.h>
#include <lock.h>
#include <net/sock.h>
#include <net/un.h>
#include <syscall.h>
#include <template_lib.h>
#include <util.h>


net::IPCSock::IPCSock(int type) : net::Socket(AF_CKIPC, type, 0) {}

net::IPCSock::~IPCSock(void) {
  // if this socket is bound, release it
  if (bindpoint != nullptr) {
    // we don't need to
    bindpoint->bound_socket = nullptr;
  }
}


ck::ref<net::Socket> net::IPCSock::accept(struct sockaddr *uaddr, int addr_len, int &err) {
  // wait on a client
  auto *client = pending_connections.recv();
  return client;
}

int net::IPCSock::connect(struct sockaddr *addr, int len) {
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


int net::IPCSock::disconnect(int flags) {
  auto &state = (flags & PFLAGS_CLIENT) ? for_server : for_client;

  state.lock.lock();
  // printk("%3d - close %s!\n", curproc->pid, (flags & PFLAGS_CLIENT) ? "client" : "server");

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


ssize_t net::IPCSock::sendto(fs::File &fd, void *data, size_t len, int flags, const sockaddr *, size_t) {
  auto &state = (fd.pflags & PFLAGS_SERVER) ? for_client : for_server;

  {
    scoped_lock l(state.lock);
    if (state.closed) {
      return 0;
    }
    // printk_nolock("%d send\n", curproc->pid);
    // printk("%3d - send message, %d live\n", curproc->pid, state.msgs.size_slow());

    state.msgs.append(ipcmsg(data, len));
  }
  state.wq.wake_up_all();

  // sched::yield();


  return len;
}



ssize_t net::IPCSock::recvfrom(fs::File &fd, void *data, size_t len, int flags, const sockaddr *, size_t) {
  auto &state = (fd.pflags & PFLAGS_SERVER) ? for_server : for_client;
  bool block = (flags & MSG_DONTWAIT) == 0;
  while (1) {
    struct wait_entry ent;
    {
      // printk("%3d - recv message, %d live. block: %d\n", curproc->pid, state.msgs.size_slow(), block);

      scoped_lock l(state.lock);


      if (flags == MSG_IPC_CLEAR) {
        state.msgs.clear();
        // return the len :)
        return len;
      }

      if (!state.msgs.is_empty()) {
        // pprintk("take data!\n");
        auto &front = state.msgs.first();
        if (flags & MSG_IPC_QUERY) {
          return front.data.size();
        }


        // the buffer needs to be big enough
        if (front.data.size() > len) return -EMSGSIZE;
        size_t nread = front.data.size();
        if (len < nread) nread = len;

        memcpy(data, front.data.data(), nread);
        state.msgs.take_first();


        return nread;
      }

      if (state.closed) {
        // printk("%3d - recv message, closed.\n", curproc->pid);
        return 0;
      }
      if (!block) return -EAGAIN;
      prepare_to_wait(state.wq, ent, true);
    }



    if (ent.start().interrupted()) {
      printk("WAIT!\n");
      return -EINTR;
    }
  }
}



int net::IPCSock::bind(const struct sockaddr *addr, size_t len) {
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


int net::IPCSock::poll(fs::File &f, int events, poll_table &pt) {
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
