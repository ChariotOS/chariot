#include <ck/func.h>
#include <ck/io.h>
#include <ck/map.h>
#include <errno.h>
#include <fcntl.h>
#include <lumen/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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
      struct await_target targ {
        0
      };
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


ck::vec<lumen::msg *> drain_messages(int fd, int &err) {
  ck::vec<uint8_t> bytes;
  for (;;) {
    uint8_t buffer[512];  // XXX dont put this on the stack
    int nread = ::recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
    if (nread == -EAGAIN) {
      break;
    }
    if (nread > 0) {
      bytes.push(buffer, nread);
    }
  }

  // printf("got %d bytes\n", bytes.size());
  // ck::hexdump(bytes.data(), bytes.size());

  ck::vec<lumen::msg *> msgs;

  uint8_t *data = bytes.data();
  for (size_t i = 0; i < bytes.size();) {
    auto *m = (lumen::msg *)(data + i);
		// if it isn't the magic number, we gotta walk to the next
		// magic number
    if (m->magic != LUMEN_MAGIC) {
      i += 1;
			fprintf(stderr, "LUMEN_MAGIC is not correct\n");
      continue;
    }

		auto total_size = sizeof(lumen::msg) + m->len;
		if (i + total_size > (size_t)bytes.size()) {
			fprintf(stderr, "malformed!\n");
			break;
		}

		auto msg = (lumen::msg*)malloc(total_size);
		memcpy(msg, data + i, total_size);

		msgs.push(msg);
		i += total_size;
  }

  return msgs;
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
    printf("event on server %02xd!\n", events);
    // READ on the server means there is someone waiting
    if (events & AWAITFS_READ) {
      int client = accept(fd, (struct sockaddr *)&addr, sizeof(addr));

      loop.register_handler(client, [](eventloop &loop, int fd, int events) {
        if (events & AWAITFS_READ) {
          int err = 0;

          auto msgs = drain_messages(fd, err);

          for (auto *msg : msgs) {
						ck::hexdump(msg->data, msg->len);
            free(msg);
          }
        }
      });
    }
  });


  while (1) {
    loop.pump();
  }

  return 0;
}
