#include <chariot/desktop_structs.h>
#include <ck/ptr.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>


class window : public ck::refcounted<window> {
  int fd = -1;
  int w, h;

  // the control block that we pass to the kernel
  struct window_control_block *m_wcb = NULL;

 public:
  static ck::ref<window> create(int w, int h);

  // do not use directly, please create
  window(int fd, int w, int h);
  ~window(void);

 private:
  // return a ref to the active wcb, creating a new one if needed
  struct window_control_block &wcb(void);
};



int get_desk_fd(void) {
  static int g_deskfd = -1;
  if (g_deskfd == -1) g_deskfd = open("/dev/desk", O_RDONLY);
  return g_deskfd;
}

ck::ref<window> window::create(int w, int h) {
	int desk = get_desk_fd();
  if (desk < 0) {
    return nullptr;
  }

  // just construct the window instance in the kernel
  int wfd = ioctl(desk, DESKTOP_CREATE_WINDOW, NULL);
  if (wfd < 0) {
    return nullptr;
  }

  return ck::make_ref<window>(wfd, w, h);
}

window::window(int fd, int w, int h) : fd(fd), w(w), h(h) {
}

window::~window(void) {
  if (fd != -1) {
    close(fd);
  }
}




int main(int argc, char **argv) {
  auto w = window::create(300, 200);
  if (!w) {
    fprintf(stderr, "failed to create window\n");
    exit(EXIT_FAILURE);
  }



  return 0;
}
