#include <cpu.h>
#include <errno.h>
#include <fs.h>
#include <lock.h>
#include <map.h>
#include <net/sock.h>
#include <sched.h>
#include <syscall.h>

net::sock::sock(uint16_t dom, int type, net::proto &p)
    : domain(dom), type(type), prot(p) {}

net::sock::~sock(void) {
  if (prot.destroy) {
    prot.destroy(*this);
  }
}

static rwlock proto_lock;
static map<int, net::proto *> protos;

void net::register_proto(net::proto &n, int af) {
  proto_lock.write_lock();

  assert(n.connect);
  assert(n.disconnect);
  assert(n.accept);
  assert(n.init);
  assert(n.destroy);

  assert(n.send);
  assert(n.recv);

  protos[af] = &n;

  proto_lock.write_unlock();
}

net::proto *net::lookup_proto(int type) {
  proto_lock.read_lock();
  net::proto *p = NULL;
  if (protos.contains(type)) {
    p = protos.get(type);
  }
  proto_lock.read_unlock();
  return p;
}

net::sock *net::sock::create(int domain, int type, int protocol, int &err) {
  err = -1;
  if (domain == PF_LOCAL || domain == PF_INET) {
    auto proto = net::lookup_proto(type);
    if (proto) {
      auto sk = new net::sock(domain, type, *proto);
      sk->protocol = protocol;

      if (!sk) return nullptr;
      proto->init(*sk);

      err = 0;
      return sk;
    }
  }

  return nullptr;
}

static int sock_seek(fs::file &, off_t old_off, off_t new_off) {
  return -EINVAL;  //
}

static ssize_t sock_read(fs::file &f, char *b, size_t s) {
  return f.ino->sk->prot.recv(*f.ino->sk, (void *)b, s);
}

static ssize_t sock_write(fs::file &f, const char *b, size_t s) {
  return f.ino->sk->prot.send(*f.ino->sk, (void *)b, s);
}

static void sock_destroy(fs::inode &f) { f.sk->prot.destroy(*f.sk); }

fs::file_operations socket_fops{
    .seek = sock_seek,
    .read = sock_read,
    .write = sock_write,

    .destroy = sock_destroy,
};

/* create an inode wrapper around a socket */
fs::inode *net::sock::createi(int domain, int type, int protocol, int &err) {
  auto sk = net::sock::create(domain, type, protocol, err);
  if (err != 0) return nullptr;

  auto ino = new fs::inode(T_SOCK, fs::DUMMY_SB);
  ino->fops = &socket_fops;
  ino->dops = NULL;

  ino->sk = sk;
  err = 0;

  return ino;
}

int sys::socket(int d, int t, int p) {
	return -1;
  int err = 0;
  auto f = net::sock::createi(d, t, p, err);
  printk("%p %d\n", f, err);
  if (err != 0) return -1;

  ref<fs::file> fd = fs::file::create(f, "socket", FDIR_READ | FDIR_WRITE);

  return curproc->add_fd(move(fd));
}
