#include <fcntl.h>
#include <gfx/image.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "internal.h"




uint32_t blend(uint32_t fgi, uint32_t bgi) {
  // only blend if we have to!
  if ((fgi & 0xFF000000) >> 24 == 0xFF) return fgi;
  if ((fgi & 0xFF000000) >> 24 == 0x00) return bgi;

  uint32_t res = 0;
  auto result = (unsigned char *)&res;
  auto fg = (unsigned char *)&fgi;
  auto bg = (unsigned char *)&bgi;

  // spooky math follows
  uint32_t alpha = fg[3] + 1;
  uint32_t inv_alpha = 256 - fg[3];
  result[0] = (unsigned char)((alpha * fg[0] + inv_alpha * bg[0]) >> 8);
  result[1] = (unsigned char)((alpha * fg[1] + inv_alpha * bg[1]) >> 8);
  result[2] = (unsigned char)((alpha * fg[2] + inv_alpha * bg[2]) >> 8);
  result[3] = 0xff;

  return res;
}


lumen::screen::screen(int w, int h) {
  fd = open("/dev/fb", O_RDWR);
  set_resolution(w, h);
  // these are leaked, but thats okay...
  cursors[mouse_cursor::pointer] = gfx::load_png("/usr/res/icons/pointer.png");
  cursors[mouse_cursor::grab] = gfx::load_png("/usr/res/icons/grab.png");
  cursors[mouse_cursor::grabbing] = gfx::load_png("/usr/res/icons/grabbing.png");
}



lumen::screen::~screen(void) {
  munmap(buf, bufsz);
  close(fd);
  buf = NULL;
}


void lumen::screen::flip_buffers(void) {
}

void lumen::screen::set_resolution(int w, int h) {
  if (buf != NULL) {
    munmap(buf, bufsz);
  }

	ck_fb_info old_info;
  ioctl(fd, FB_GET_INFO, &old_info);


  info.width = w;
  info.height = h;
	if (ioctl(fd, FB_GET_INFO, &info) < 0) {
		printf("[lumen]: failed to set resolution, loading existing state\n");
		printf("[lumen]: w: %ld, h: %ld\n", old_info.width, old_info.height);
		info = old_info;
	}
  m_bounds = gfx::rect(0, 0, info.width, info.height);
  mouse_pos.constrain(m_bounds);


  bufsz = info.width * info.height * sizeof(uint32_t) * 2;
  buf =
      (uint32_t *)mmap(NULL, bufsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  buffer_index = 0;
  front_buffer = buf;
  back_buffer = buf + (width() * height());
}



long xoff = 0;
long yoff = 0;
long delta = 10;
const gfx::point &lumen::screen::handle_mouse_input(struct mouse_packet &pkt) {

  mouse_pos.set_x(mouse_pos.x() + pkt.dx);
  mouse_pos.set_y(mouse_pos.y() + pkt.dy);
  /* The mouse is constrained within the visible region */
  gfx::rect sc(0, 0, width() - 1, height() - 1);
  mouse_pos.constrain(sc);
	mouse_moved = true;

  // TODO: buttons

  return mouse_pos;
}


inline int abs(int a) { return a < 0 ? -a : a; }
inline int min(int a, int b) { return a < b ? a : b; }


void draw_rect(lumen::screen &s, const gfx::rect &bound, uint32_t color) {
  auto l = bound.left();
  auto t = bound.top();

  for (int y = 0; y < bound.h; y++) {
    for (int x = 0; x < bound.w; x++) {
      s.set_pixel(x + l, y + t, color);
    }
  }
}

void lumen::screen::draw_mouse(void) {
  auto &cur = cursors[cursor];

  // draw the cursor to the framebuffer (NOT THE DOUBLE BUFFER!!!)
  auto draw_rect = mouse_rect();

  auto right = draw_rect.right();
  auto left = draw_rect.left();
  auto top = draw_rect.top();
  auto bottom = draw_rect.bottom();
  auto w = width();

	auto buf = buffer();

  for (int y = top; y < bottom; y++) {
    if (y < 0 || y >= height()) continue;
    for (int x = left; x < right; x++) {
      if (x < 0 || x >= w) continue;

      uint32_t pix = cur->get_pixel(x - left, y - top);
			if (pix == 0xFFFF00FF) {
				pix = 0xFF'FFFFFF;
			} else if ((pix & 0xFF000000) >> 24 != 0xFF) {
				continue;
        /* This is slow */
        uint32_t bg = buf[x + y * w];
        pix = blend(pix, bg);
      }
      buf[x + y * w] = pix;
    }
  }

}
