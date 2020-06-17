#include <ck/io.h>
#include <lumen.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


lumen::session::session(void) { sock.connect("/usr/servers/lumen"); }

lumen::session::~session(void) {
  // TODO: send the shutdown message
}


static unsigned long nextmsgid(void) {
  static unsigned long sid = 0;

  return sid++;
}

long lumen::session::send_raw(int type, void *payload, size_t payloadsize) {
  size_t msgsize = payloadsize + sizeof(lumen::msg);
  auto msg = (lumen::msg *)malloc(msgsize);

  msg->magic = LUMEN_MAGIC;
  msg->type = type;
  msg->id = nextmsgid();
  msg->len = payloadsize;

  if (payloadsize > 0) memcpy(msg + 1, payload, payloadsize);

  auto w = sock.write((const void *)msg, msgsize);

  free(msg);

  return w;
}

