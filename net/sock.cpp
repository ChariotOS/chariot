#include <cpu.h>
#include <errno.h>
#include <fs.h>
#include <lock.h>
#ifdef CONFIG_LWIP
#include <lwip/netdb.h>
#endif
#include <ck/map.h>
#include <net/sock.h>
#include <sched.h>
#include <syscall.h>
#include <util.h>
#include "mm.h"

net::Socket::Socket(int dom, int type, int protocol) : fs::Node(nullptr), domain(dom), type(type), protocol(protocol) {}

net::Socket::~Socket(void) {}

extern net::Socket *udp_create(int domain, int type, int protocol, int &err);

ck::ref<net::Socket> net::Socket::create(int domain, int type, int protocol, int &err) {
#ifdef CONFIG_LWIP
  // manually
  if (domain == PF_INET) {
    err = 0;
    return new net::ip4sock(type);
  }
#endif

  if (domain == PF_LOCAL) {
    if (type == SOCK_STREAM) {
      err = 0;
      return ck::make_ref<net::LocalSocket>(type);
    }
  }


  if (domain == PF_CKIPC) {
    if (type == SOCK_DGRAM) {
      err = 0;
      return ck::make_ref<net::IPCSock>(type);
    }
  }

  err = -EINVAL;
  return nullptr;
}

static int sock_seek(fs::File &, off_t old_off, off_t new_off) {
  return -EINVAL;  //
}


void net::Socket::close(fs::File &fd) { disconnect(fd.pflags); }


int net::Socket::poll(fs::File &fd, int events, poll_table &pt) { return 0; }

/* create an inode wrapper around a socket */
ck::ref<fs::Node> net::Socket::createi(int domain, int type, int protocol, int &err) {
  // printk("domain=%3d, type=%3d, proto=%3d\n", domain, type, protocol);
  auto sk = net::Socket::create(domain, type, protocol, err);
  // printk("sk %p %d\n", sk, err);
  if (err != 0) return nullptr;

  return sk;
}

int sys::socket(int d, int t, int p) {
  int err = 0;
  auto f = net::Socket::createi(d, t, p, err);
  // printk("in %p %d\n", f, err);
  if (err != 0) return -1;

  ck::ref<fs::File> fd = fs::File::create(f, "socket", FDIR_READ | FDIR_WRITE);

  return curproc->add_fd(move(fd));
}

ssize_t sys::sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen) {
  if (!VALIDATE_RD((void *)buf, len)) return -EINVAL;

  if (dest_addr != NULL) {
    if (!VALIDATE_RD((void *)dest_addr, addrlen)) {
      return -EINVAL;
    }
  }

  ck::ref<fs::File> file = curproc->get_fd(sockfd);


  // printk("[%3d] sendto as '%s'\n", curproc->pid, file->pflags == PFLAGS_CLIENT ? "client" :
  // "server");

  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->is_sock()) {
      auto *sock = (net::Socket *)file->ino.get();
      res = sock->sendto(*file, (void *)buf, len, flags, dest_addr, addrlen);
    }
  }

  return res;
}


ssize_t sys::recvfrom(int sockfd, void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen) {
  if (!VALIDATE_WR((void *)buf, len)) return -EINVAL;

  if (dest_addr != NULL) {
    if (!VALIDATE_RD((void *)dest_addr, addrlen)) {
      return -EINVAL;
    }
  }

  ck::ref<fs::File> file = curproc->get_fd(sockfd);


  // printk("[%3d] recvfrom as '%s'\n", curproc->pid,
  //     file->pflags == PFLAGS_CLIENT ? "client" : "server");


  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->is_sock()) {
      auto *sock = (net::Socket *)file->ino.get();
      res = sock->recvfrom(*file, (void *)buf, len, flags, dest_addr, addrlen);
    }
  }

  return res;
}

int sys::bind(int sockfd, const struct sockaddr *addr, size_t len) {
  if (!curproc->mm->validate_pointer((void *)addr, len, VALIDATE_READ)) {
    return -EINVAL;
  }

  ck::ref<fs::File> file = curproc->get_fd(sockfd);
  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->is_sock()) {
      auto *sock = (net::Socket *)file->ino.get();
      res = sock->bind(addr, len);
    } else {
      res = -ENOTSOCK;
    }
  }

  return res;
}


int sys::accept(int sockfd, struct sockaddr *addr, size_t addrlen) {
  if (!curproc->mm->validate_pointer((void *)addr, addrlen, VALIDATE_READ)) {
    return -EINVAL;
  }

  ck::ref<fs::File> file = curproc->get_fd(sockfd);
  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->is_sock()) {
      auto *sock = (net::Socket *)file->ino.get();

      int err = 0;
      auto sk = sock->accept((struct sockaddr *)addr, addrlen, err);
      if (sk != nullptr) {
        int fd = curproc->add_fd(fs::File::create(sk, "[socket]"));

        return fd;

      } else {
        return err;
      }
    } else {
      res = -ENOTSOCK;
    }
  }

  return res;
}


int sys::connect(int sockfd, const struct sockaddr *addr, size_t len) {
  if (!curproc->mm->validate_pointer((void *)addr, len, VALIDATE_READ)) {
    return -EINVAL;
  }


  ck::ref<fs::File> file = curproc->get_fd(sockfd);
  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->is_sock()) {
      auto *sk = (net::Socket *)file->ino.get();
      res = sk->connect((struct sockaddr *)addr, len);
      if (res < 0) {
        sk->connected = true;
        return res;
      }
      sk->connected = false;
      file->pflags = PFLAGS_CLIENT;
    } else {
      res = -ENOTSOCK;
    }
  }

  return res;
}



int net::Socket::ioctl(fs::File &f, unsigned int cmd, off_t arg) { return -ENOTIMPL; }

int net::Socket::connect(struct sockaddr *uaddr, int addr_len) { return -ENOTIMPL; }


int net::Socket::disconnect(int flags) { return -ENOTIMPL; }

ck::ref<net::Socket> net::Socket::accept(struct sockaddr *uaddr, int addr_len, int &err) {
  err = -ENOTIMPL;
  return nullptr;
}

ssize_t net::Socket::sendto(fs::File &, void *data, size_t len, int flags, const sockaddr *, size_t) { return -ENOTIMPL; }
ssize_t net::Socket::recvfrom(fs::File &, void *data, size_t len, int flags, const sockaddr *, size_t) { return -ENOTIMPL; }


int net::Socket::bind(const struct sockaddr *addr, size_t len) { return -ENOTIMPL; }



static spinlock dnslookup_lock;
/* This is a pretty dumb function... There is a bunch more information we are missing */
int sys::dnslookup(const char *name, unsigned int *ip4) {
  auto proc = cpu::proc();
#ifdef CONFIG_LWIP

  if (!proc->mm->validate_string(name)) {
    return -EINVAL;
  }

  if (!proc->mm->validate_pointer(ip4, 4, VALIDATE_WRITE)) {
    return -EINVAL;
  }
  /* we need a copy of the name in the kernel because the LWIP thread doesn't have access to
   * userspace */
  string s = name;

  scoped_lock l(dnslookup_lock);

  struct hostent *phe = lwip_gethostbyname(s.get());

  if (phe == NULL) {
    return -ENOENT;
  }
  if (phe->h_length != 4) {
    return -E2BIG;
  }

  memcpy((char *)ip4, phe->h_addr, 4);

  return 0;
#else
  return -ENOSYS;
#endif
}
