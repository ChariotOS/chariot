#include <ck/vec.h>
#include <errno.h>
#include <lumen.h>
#include <lumen/msg.h>
#include <unistd.h>

ck::vec<lumen::msg *> lumen::drain_messages(ck::IPCSocket &sock, bool &failed) {
  failed = false;
  ck::vec<lumen::msg *> msgs;

  while (1) {
    uint8_t buf;
    int sz = sock.recv(&buf, 1, MSG_IPC_QUERY | MSG_DONTWAIT);

    if (sz < 0 || sock.eof()) {
      if (errno == EAGAIN) break;
      failed = true;
      break;
    }

    char *buffer = (char *)malloc(sz);
    int nread = sock.recv(buffer, sz, 0);
    if (nread != sz || sock.eof()) {
      free(buffer);
      failed = true;
      break;
    }
    auto *msg = (lumen::msg *)buffer;
    msgs.push(msg);
  }



  return msgs;
}
