#include <ck/func.h>
#include <ck/io.h>
#include <ck/map.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <lumen/msg.h>

#include "internal.h"

#include <chariot/awaitfs_types.h>
#include <sys/syscall.h>

int awaitfs(struct await_target *fds, int nfds, int flags) {
  return errno_syscall(SYS_awaitfs, fds, nfds, flags);
}



class eventloop {
  using callback_t = ck::func<void(eventloop &, int fd, int events)>;

  struct handler {
    callback_t cb;

    handler(callback_t cb) : cb(cb) {}
    handler() : cb([](eventloop &, int fd, int events) {}) {}
  };

  ck::map<int, handler> handlers;

 public:
  eventloop(void) {}

  void register_handler(int fd, callback_t cb) {
    handlers.set(fd, handler(cb));
  }

  void deregister(int fd) { handlers.remove(fd); }

  void pump(void) {
    ck::vec<struct await_target> targs;

    for (auto &kv : handlers) {
      struct await_target targ {0};
      targ.fd = kv.key;
      targ.awaiting = AWAITFS_ALL;

      targs.push(targ);
    }

    int index = awaitfs(targs.data(), targs.size(), 0);
    if (index >= 0) {
      int fd = targs[index].fd;
      auto &handler = handlers[fd];
      handler.cb(*this, fd, targs[index].occurred);
    }
  }
};



char upper(char ch) {
  if (ch >= 'a' && ch <= 'z')
    return ('A' + ch - 'a');
  else
    return ch;
}



struct lumen::msg *read_msg(int fd, int &err) {
  // read in the base data
  struct lumen::msg base;

	while (1) {
		int n = read(fd, &base.magic, sizeof(int));
		ck::hexdump(&base.magic, sizeof(int));
		if (n < 0) {
			err = n;
			return NULL;
		}

		if (base.magic == LUMEN_MAGIC) break;
		printf("magic is not right!\n");
	}

  int n = read(fd, &base.type, sizeof(base) - sizeof(int));
  if (n < 0) {
    err = n;
    return NULL;
  }
  // TODO: this could break stuff!
  if (n <= 0) return NULL;

  auto *msg = (lumen::msg *)malloc(sizeof(lumen::msg) + base.len);
  msg->type = base.type;
  msg->id = base.id;
  msg->len = base.len;



	auto *buf = (char*)(msg + 1);
	int nread = 0;
	while (nread != base.len) {
    int n = read(fd, buf + nread, msg->len);
    if (n < 0) {
      free(msg);
      err = n;
      return NULL;
    }
		nread += n;
	}
  err = 0;
  return msg;
}



int main(int argc, char **argv) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;

  // bind the local socket to the windowserver
  strncpy(addr.sun_path, "/usr/servers/lumen", sizeof(addr.sun_path) - 1);
  int bind_res = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (bind_res < 0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  eventloop loop;

  loop.register_handler(fd, [&](eventloop &loop, int fd, int events) {
    // READ on the server means there is someone waiting
    if (events & AWAITFS_READ) {
      int client = accept(fd, (struct sockaddr *)&addr, sizeof(addr));

      loop.register_handler(client, [](eventloop &loop, int fd, int events) {
        if (events & AWAITFS_READ) {
          int err = 0;
          auto *msg = read_msg(fd, err);
          if (err != 0) {
            printf("er=%d\n", err);
						// close(client);
            return;
          }

          for (int i = 0; i < msg->len; i++) {
            printf("%4lu: 0x%02x\n", msg->id, msg->data[i]);
          }

          free(msg);
        }
      });
    }
  });


  while (1) {
    loop.pump();
  }

  return 0;
}
