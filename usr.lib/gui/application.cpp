#include <ck/io.h>
#include <errno.h>
#include <lumen.h>
#include <lumen/msg.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <gui/application.h>

static gui::application *the_app = NULL;

gui::application &gui::application::get(void) { return *the_app; }

gui::application::application(void) {
  if (the_app != NULL) {
    fprintf(stderr, "Cannot have multiple gui::applications\n");
    exit(EXIT_FAILURE);
  }

  the_app = this;
  sock.connect("/usr/servers/lumen");

  sock.on_read([this] {
    drain_messages();
    dispatch_messages();
  });


  struct lumen::greet_msg greet = {0};
  struct lumen::greetback_msg greetback = {0};

  greet.pid = getpid();
  if (send_msg_sync(LUMEN_MSG_GREET, greet, &greetback)) {
		ck::hexdump(greetback);
    printf("my client id is %d!\n", greetback.client_id);
  }
}

gui::application::~application(void) {
  the_app = NULL;
  // TODO: send the shutdown message
}


static unsigned long nextmsgid(void) {
  static unsigned long sid = 0;

  return sid++;
}

long gui::application::send_raw(int type, void *payload, size_t payloadsize) {
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


lumen::msg *gui::application::send_raw_sync(int type, void *payload,
                                            size_t payloadsize) {
  size_t msgsize = payloadsize + sizeof(lumen::msg);
  auto msg = (lumen::msg *)malloc(msgsize);

  msg->magic = LUMEN_MAGIC;
  msg->type = type;
  msg->id = nextmsgid();
  msg->len = payloadsize;

  if (payloadsize > 0) memcpy(msg + 1, payload, payloadsize);

  // TODO: this might fail?
  sock.write((const void *)msg, msgsize);

  lumen::msg *response = NULL;

  // wait for a response (this can be smarter)
  while (response == NULL) {
    bool failed = false;
    auto msgs = lumen::drain_messages(sock, failed);
    if (failed) break;

    for (auto *got : msgs) {
      if (got->id == msg->id) {
        response = got;
      } else {
        m_pending_messages.push(msg);
      }
    }
  }

  free(msg);
  return response;
}



void gui::application::drain_messages(void) {
  bool failed = false;
  auto msgs = lumen::drain_messages(sock, failed);

  for (auto *msg : msgs) {
    m_pending_messages.push(msg);
  }
}


void gui::application::dispatch_messages(void) {
  for (auto *msg : m_pending_messages) {
    printf("unhandled message %d (%p)\n", msg->id, msg);
    delete msg;
  }
}


void gui::application::start(void) { m_eventloop.start(); }


ck::ref<gui::window> gui::application::new_window(ck::string name, int w,
                                                  int h) {

	struct lumen::create_window_msg msg;
	msg.width = w;
	msg.height = h;
  strncpy(msg.name, name.get(), LUMEN_NAMESZ - 1);

	// the response message
	struct lumen::window_created_msg res = {0};

	if (send_msg_sync(LUMEN_MSG_CREATE_WINDOW, msg, &res)) {
		if (res.window_id >= 0) {
			return gui::window::create(res.window_id, name, gfx::rect(w, h, 0, 0));
		}
		printf("window_id = %d\n", res.window_id);
	}

  return nullptr;
}
