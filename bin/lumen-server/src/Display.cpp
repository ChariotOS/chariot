#include <lumen-server/Display.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

namespace server {


  Display::Display(WindowManager& wm) : wm(wm) {
    fd = open("/dev/video0", O_RDWR);
    if (fd == -1) {
      /* TODO: do this somewhere else? */
      fprintf(stderr, "lumen-server: Failed to open /dev/video0. (do you have a graphics device?)\n");
      exit(EXIT_FAILURE);
    }
    /* TRUST that /dev/video0 is a gvi device :^) */

    int res = ioctl(fd, GVI_GET_MODE, &info);

    m_bounds = gfx::rect(0, 0, info.width, info.height);
    mouse_pos.constrain(m_bounds);

    bufsz = info.width * info.height * sizeof(uint32_t);

    buffer_index = 0;
    front_buffer = (uint32_t*)mmap(NULL, bufsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    back_buffer = NULL;

    if (!hardware_double_buffered()) {
      back_buffer = (uint32_t*)mmap(NULL, bufsz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    }

		// Clear the display and flush it to get rid of the initial kernel logging
    clear(0x00000000);
    flush_fb(FlushWithMemcpy::Yes);
  }


  Display::~Display() {
    if (back_buffer) {
      munmap(back_buffer, bufsz);
    }
    munmap(front_buffer, bufsz);
    close(fd);
  }


  void Display::flush_fb(FlushWithMemcpy with_memcpy) {
    if (hardware_double_buffered()) ioctl(fd, GVI_FLUSH_FB);

    if (with_memcpy == FlushWithMemcpy::Yes) {
      memcpy(front_buffer, back_buffer, bufsz);
    }
  }



  gfx::bitmap* Display::bmp(void) { return NULL; }

  void Display::invalidate(const gfx::rect& r) {}

  ck::pair<int, int> Display::resize(int width, int height) { return {0, 0}; }

  ui::view* Display::root_view(void) { return NULL; }

  void Display::did_reflow(void) {}


}  // namespace server
