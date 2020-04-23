#include <cpu.h>
#include <errno.h>
#include <fs.h>
#include <lock.h>
#include <map.h>
#include <net/sock.h>
#include <sched.h>
#include <syscall.h>

net::sock::sock(int dom, int type, int protocol)
    : domain(dom), type(type), protocol(protocol) {}

net::sock::~sock(void) {}

extern net::sock *udp_create(int domain, int type, int protocol, int &err);

net::sock *net::sock::create(int domain, int type, int protocol, int &err) {
  // manually
  if (domain == PF_INET) {
    if (type == SOCK_DGRAM) {
      err = 0;
      return new net::udpsock(domain, type, protocol);
    }
  }

  err = -EINVAL;
  return NULL;
}

static int sock_seek(fs::file &, off_t old_off, off_t new_off) {
  return -EINVAL;  //
}

static ssize_t sock_read(fs::file &f, char *b, size_t s) {
  return f.ino->sk->recvfrom((void *)b, s, 0, nullptr, 0);
}

static ssize_t sock_write(fs::file &f, const char *b, size_t s) {
  return f.ino->sk->sendto((void *)b, s, 0, nullptr, 0);
}

static void sock_destroy(fs::inode &f) {
  delete f.sk;
  f.sk = NULL;
}

fs::file_operations socket_fops{
    .seek = sock_seek,
    .read = sock_read,
    .write = sock_write,

    .destroy = sock_destroy,
};

/* create an inode wrapper around a socket */
fs::inode *net::sock::createi(int domain, int type, int protocol, int &err) {
  // printk("domain=%3d, type=%3d, proto=%3d\n", domain, type, protocol);
  auto sk = net::sock::create(domain, type, protocol, err);
	// printk("sk %p %d\n", sk, err);
  if (err != 0) return nullptr;

  auto ino = new fs::inode(T_SOCK, fs::DUMMY_SB);
  ino->fops = &socket_fops;
  ino->dops = NULL;

  ino->sk = sk;
  err = 0;

  return ino;
}

int sys::socket(int d, int t, int p) {
  int err = 0;
  auto f = net::sock::createi(d, t, p, err);
	// printk("in %p %d\n", f, err);
  if (err != 0) return -1;

  ref<fs::file> fd = fs::file::create(f, "socket", FDIR_READ | FDIR_WRITE);

  return curproc->add_fd(move(fd));
}

ssize_t sys::sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, size_t addrlen) {

	if (!curproc->mm->validate_pointer((void*)buf, len, VALIDATE_READ)) {
		return -EINVAL;
	}

	if (!curproc->mm->validate_pointer((void*)dest_addr, addrlen, VALIDATE_READ)) {
		return -EINVAL;
	}

  ref<fs::file> file = curproc->get_fd(sockfd);
	ssize_t res = -EINVAL;
  if (file) {
		if (file->ino->type == T_SOCK) {
			res = file->ino->sk->sendto((void*)buf, len, flags, dest_addr, addrlen);
		}
  }

	return res;
}

int net::sock::ioctl(int cmd, unsigned long arg) { return -ENOTIMPL; }

int net::sock::connect(struct sockaddr *uaddr, int addr_len) {
  return -ENOTIMPL;
}

int net::sock::disconnect(int flags) { return -ENOTIMPL; }

net::sock *net::sock::accept(int flags, int &err) {
  err = -ENOTIMPL;
  return NULL;
}

ssize_t net::sock::sendto(void *data, size_t len, int flags, const sockaddr *, size_t) {
  return -ENOTIMPL;
}
ssize_t net::sock::recvfrom(void *data, size_t len, int flags, const sockaddr *, size_t) {
  return -ENOTIMPL;
}
