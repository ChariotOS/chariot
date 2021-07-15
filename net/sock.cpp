#include <cpu.h>
#include <errno.h>
#include <fs.h>
#include <lock.h>
#ifdef CONFIG_LWIP
#include <lwip/netdb.h>
#endif
#include <map.h>
#include <net/sock.h>
#include <sched.h>
#include <syscall.h>
#include <util.h>
#include "mm.h"

net::sock::sock(int dom, int type, int protocol) : domain(dom), type(type), protocol(protocol) {}

net::sock::~sock(void) {}

extern net::sock *udp_create(int domain, int type, int protocol, int &err);

net::sock *net::sock::create(int domain, int type, int protocol, int &err) {
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
      return new net::localsock(type);
    }
  }


  if (domain == PF_CKIPC) {
    if (type == SOCK_DGRAM) {
      err = 0;
      return new net::ipcsock(type);
    }
  }

  err = -EINVAL;
  return NULL;
}

static int sock_seek(fs::file &, off_t old_off, off_t new_off) {
  return -EINVAL;  //
}

static ssize_t sock_read(fs::file &f, char *b, size_t s) {
  // printk("[%3d] recvfrom as '%s'\n", curproc->pid, f.pflags == PFLAGS_CLIENT ? "client" :
  // "server");
  if (f.ino->sk->connected) return -ENOTCONN;
  return f.ino->sk->recvfrom(f, (void *)b, s, 0, nullptr, 0);
}

static ssize_t sock_write(fs::file &f, const char *b, size_t s) {
  // printk("[%3d] sendto as '%s'\n", curproc->pid, f.pflags == PFLAGS_CLIENT ? "client" :
  // "server");
  if (f.ino->sk->connected) return -ENOTCONN;
  return f.ino->sk->sendto(f, (void *)b, s, 0, nullptr, 0);
}

static void sock_close(fs::file &fd) {
  auto f = *fd.ino;
  if (f.sk) {
    f.sk->disconnect(fd.pflags);
  }
  net::sock::release(f.sk);
  f.sk = NULL;
}


static int sock_poll(fs::file &fd, int events, poll_table &pt) {
  auto f = *fd.ino;
  if (f.sk) {
    return f.sk->poll(fd, events, pt);
  }
  return 0;
}

fs::file_operations socket_fops{
    .seek = sock_seek,
    .read = sock_read,
    .write = sock_write,

    .close = sock_close,

    .poll = sock_poll,
};


net::sock *net::sock::acquire(net::sock &sk) {
  sk.owners_lock.lock();
  sk.owners += 1;
  sk.owners_lock.unlock();
  return &sk;
}


void net::sock::release(net::sock *&sk) {
  if (sk == NULL) return;
  sk->owners_lock.lock();
  sk->owners--;
  if (sk->owners == 0) {
    delete sk;
  } else {
    sk->owners_lock.unlock();
  }
  // make sure the owner can't access their reference anymroe,
  sk = NULL;
}

/* create an inode wrapper around a socket */
fs::inode *net::sock::createi(int domain, int type, int protocol, int &err) {
  // printk("domain=%3d, type=%3d, proto=%3d\n", domain, type, protocol);
  auto sk = net::sock::create(domain, type, protocol, err);
  // printk("sk %p %d\n", sk, err);
  if (err != 0) return nullptr;

  auto ino = new fs::inode(T_SOCK, fs::DUMMY_SB);
  ino->fops = &socket_fops;
  ino->dops = NULL;

  ino->sk = net::sock::acquire(*sk);
  sk->ino = ino;
  err = 0;

  return ino;
}

int sys::socket(int d, int t, int p) {
  int err = 0;
  auto f = net::sock::createi(d, t, p, err);
  // printk("in %p %d\n", f, err);
  if (err != 0) return -1;

  ck::ref<fs::file> fd = fs::file::create(f, "socket", FDIR_READ | FDIR_WRITE);

  return curproc->add_fd(move(fd));
}

ssize_t sys::sendto(int sockfd, const void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, size_t addrlen) {
  if (!VALIDATE_RD((void *)buf, len)) return -EINVAL;

  if (dest_addr != NULL) {
    if (!VALIDATE_RD((void *)dest_addr, addrlen)) {
      return -EINVAL;
    }
  }

  ck::ref<fs::file> file = curproc->get_fd(sockfd);


  // printk("[%3d] sendto as '%s'\n", curproc->pid, file->pflags == PFLAGS_CLIENT ? "client" :
  // "server");

  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->type == T_SOCK) {
      res = file->ino->sk->sendto(*file, (void *)buf, len, flags, dest_addr, addrlen);
    }
  }

  return res;
}


ssize_t sys::recvfrom(int sockfd, void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, size_t addrlen) {
  if (!VALIDATE_WR((void *)buf, len)) return -EINVAL;

  if (dest_addr != NULL) {
    if (!VALIDATE_RD((void *)dest_addr, addrlen)) {
      return -EINVAL;
    }
  }

  ck::ref<fs::file> file = curproc->get_fd(sockfd);


  // printk("[%3d] recvfrom as '%s'\n", curproc->pid, file->pflags == PFLAGS_CLIENT ? "client" :
  // "server");


  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->type == T_SOCK) {
      res = file->ino->sk->recvfrom(*file, (void *)buf, len, flags, dest_addr, addrlen);
    }
  }

  return res;
}

int sys::bind(int sockfd, const struct sockaddr *addr, size_t len) {
  if (!curproc->mm->validate_pointer((void *)addr, len, VALIDATE_READ)) {
    return -EINVAL;
  }

  ck::ref<fs::file> file = curproc->get_fd(sockfd);
  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->type == T_SOCK) {
      res = file->ino->sk->bind(addr, len);
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

  ck::ref<fs::file> file = curproc->get_fd(sockfd);
  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->type == T_SOCK) {
      int err = 0;
      auto sk = file->ino->sk->accept((struct sockaddr *)addr, addrlen, err);
      if (sk != NULL) {
        auto ino = new fs::inode(T_SOCK, fs::DUMMY_SB);
        ino->fops = &socket_fops;
        ino->dops = NULL;
        ino->sk = net::sock::acquire(*sk);
        sk->ino = ino;
        auto file = fs::file::create(ino, "[socket]", O_RDWR);
        file->pflags = PFLAGS_SERVER;
        int fd = curproc->add_fd(file);

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


  ck::ref<fs::file> file = curproc->get_fd(sockfd);
  ssize_t res = -EINVAL;
  if (file) {
    if (file->ino->type == T_SOCK) {
      res = file->ino->sk->connect((struct sockaddr *)addr, len);
      if (res < 0) {
        file->ino->sk->connected = true;
        return res;
      }
      file->ino->sk->connected = false;
      file->pflags = PFLAGS_CLIENT;
    } else {
      res = -ENOTSOCK;
    }
  }

  return res;
}



int net::sock::ioctl(int cmd, unsigned long arg) { return -ENOTIMPL; }

int net::sock::connect(struct sockaddr *uaddr, int addr_len) { return -ENOTIMPL; }


int net::sock::disconnect(int flags) { return -ENOTIMPL; }

net::sock *net::sock::accept(struct sockaddr *uaddr, int addr_len, int &err) {
  err = -ENOTIMPL;
  return NULL;
}

ssize_t net::sock::sendto(fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t) {
  return -ENOTIMPL;
}
ssize_t net::sock::recvfrom(
    fs::file &, void *data, size_t len, int flags, const sockaddr *, size_t) {
  return -ENOTIMPL;
}


int net::sock::bind(const struct sockaddr *addr, size_t len) { return -ENOTIMPL; }



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
