#include <errno.h>
#include <lock.h>
#include <net/sock.h>

static rwlock all_localsocks_lock;
static struct net::localsock *all_localsocks = NULL;

net::localsock::localsock(int type) : net::sock(AF_LOCAL, type, 0) {
  all_localsocks_lock.write_lock();
  next = all_localsocks;
  prev = nullptr;
  all_localsocks = this;
  if (next != NULL) {
    next->prev = this;
  }
  all_localsocks_lock.write_unlock();
}

net::localsock::~localsock(void) {
  all_localsocks_lock.write_lock();
  if (all_localsocks == this) {
    all_localsocks = next;
  }

  if (next) next->prev = prev;
  if (prev) prev->next = next;
  all_localsocks_lock.write_unlock();
}

int net::localsock::connect(struct sockaddr *uaddr, int addr_len) {
  return -ENOTIMPL;
}
int net::localsock::disconnect(int flags) { return -ENOTIMPL; }
net::sock *net::localsock::accept(int flags, int &error) { return NULL; }

// implemented by the network layer (OSI)
ssize_t net::localsock::sendto(void *data, size_t len, int flags,
			       const sockaddr *, size_t) {
  return -ENOTIMPL;
}
ssize_t net::localsock::recvfrom(void *data, size_t len, int flags,
				 const sockaddr *, size_t) {
  return -ENOTIMPL;
}

int net::localsock::bind(const struct sockaddr *addr, size_t len) {
  return -ENOTIMPL;
}

