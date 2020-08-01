#include <ck/io.h>
#include <errno.h>
#include <lumen.h>
#include <lumen/msg.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <ui/application.h>

static ui::application *the_app = NULL;

ui::application &ui::application::get(void) { return *the_app; }

ui::application::application(void) {
  if (the_app != NULL) {
    fprintf(stderr, "Cannot have multiple ui::applications\n");
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
    printf("my guest id is %d!\n", greetback.guest_id);
  }
}

ui::application::~application(void) {
  the_app = NULL;
  // TODO: send the shutdown message
}


static unsigned long nextmsgid(void) {
  static unsigned long sid = 0;

  return sid++;
}

long ui::application::send_raw(int type, void *payload, size_t payloadsize) {
  size_t msgsize = payloadsize + sizeof(lumen::msg);
  auto msg = (lumen::msg *)malloc(msgsize);

  msg->magic = LUMEN_MAGIC;
  msg->type = type;
  msg->id = nextmsgid();
  msg->len = payloadsize;

  if (payloadsize > 0) memcpy(msg + 1, payload, payloadsize);


	// printf("\033[31;1m[client send] id: %6d. type: %04x\033[31;0m\n", msg->id, msg->type);
  auto w = sock.write((const void *)msg, msgsize);

  free(msg);
  return w;
}


lumen::msg *ui::application::send_raw_sync(int type, void *payload,
                                            size_t payloadsize) {
  size_t msgsize = payloadsize + sizeof(lumen::msg);
  auto msg = (lumen::msg *)malloc(msgsize);

  msg->magic = LUMEN_MAGIC;
  msg->type = type;
  msg->id = nextmsgid();
  msg->len = payloadsize;

  if (payloadsize > 0) memcpy(msg + 1, payload, payloadsize);

	// printf("\033[31;1m[client send] id: %6d. type: %04x\033[0m\n", msg->id, msg->type);
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
        m_pending_messages.push(got);
      }
    }
  }

  free(msg);
  return response;
}



void ui::application::drain_messages(void) {
  bool failed = false;
  auto msgs = lumen::drain_messages(sock, failed);

  for (auto *msg : msgs) {
    m_pending_messages.push(msg);
  }
}


void ui::application::dispatch_messages(void) {
  for (auto *msg : m_pending_messages) {
		if (msg->type == LUMEN_MSG_INPUT) {
			auto *inp = (struct lumen::input_msg*)(msg + 1);

			int wid = inp->window_id;
			if (m_windows.contains(wid)) {
				auto *win = m_windows.get(wid).get();
				assert(win != NULL);
				win->handle_input(*inp);
			} else {
				printf("Got n input message from the window server for a window I don't control! (wid=%d)\n", wid);
			}
		} else {
    	printf("unhandled message %d (%p)\n", msg->id, msg);
		}

    delete msg;
  }
	m_pending_messages.clear();
}


void ui::application::start(void) { m_eventloop.start(); }


ui::window *ui::application::new_window(ck::string name, int w,
                                                  int h) {
  struct lumen::create_window_msg msg;
  msg.width = w;
  msg.height = h;
  strncpy(msg.name, name.get(), LUMEN_NAMESZ - 1);

  // the response message
  struct lumen::window_created_msg res = {0};

  if (send_msg_sync(LUMEN_MSG_CREATE_WINDOW, msg, &res)) {
    if (res.window_id >= 0) {
			auto win = ck::make_unique<ui::window>(res.window_id, name, gfx::rect(0, 0, w, h), gfx::shared_bitmap::get(res.bitmap_name, w, h));
			m_windows.set(res.window_id, move(win));
			return m_windows.get(res.window_id).get();
    }
  }

  return nullptr;
}
