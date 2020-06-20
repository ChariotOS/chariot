#include <fcntl.h>
#include <gfx/image.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "internal.h"




uint32_t blend(uint32_t fgi, uint32_t bgi) {
  // only blend if we have to!
  if ((fgi & 0xFF000000) >> 24 == 0xFF) {
    return fgi;
  }


  if ((fgi & 0xFF000000) >> 24 == 0x00) {
    return bgi;
  }

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

void draw_bmp_scaled(ck::ref<gfx::bitmap> bmp, lumen::screen &screen, int xo,
                     int yo, float scale) {}


void draw_bmp(ck::ref<gfx::bitmap> bmp, lumen::screen &screen, int xo, int yo) {
  if (bmp) {
    for (size_t y = 0; y < bmp->height(); y++) {
      for (size_t x = 0; x < bmp->width(); x++) {
        uint32_t bp = bmp->get_pixel(x, y);
        uint32_t sp = screen.get_pixel(x + xo, y + yo);
        uint32_t pix = blend(bp, sp);
        screen.set_pixel(x + xo, y + yo, pix);
      }
    }
  }
}

lumen::screen::screen(int w, int h) {
  fd = open("/dev/fb", O_RDWR);
  back_bitmap = gfx::shared_bitmap::create(w, h);
  set_resolution(w, h);

  // these are leaked, but thats okay...
  cursors[mouse_cursor::pointer] = gfx::load_png("/usr/res/icons/pointer.png");
}

lumen::screen::~screen(void) {
  munmap(buf, bufsz);
  close(fd);
  buf = NULL;
}

void lumen::screen::set_resolution(int w, int h) {
  if (buf != NULL) {
    munmap(buf, bufsz);
  }


  info.width = w;
  info.height = h;
  m_bounds = gfx::rect(0, 0, w, h);
  mouse_pos.constrain(m_bounds);

  flush_info();

  bufsz = w * h * sizeof(uint32_t);
  buf =
      (uint32_t *)mmap(NULL, bufsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
}



void *mmx_memcpy(void *dest, const void *src, size_t len) {
  ASSERT(len >= 1024);

  auto *dest_ptr = (uint8_t *)dest;
  auto *src_ptr = (const uint8_t *)src;

  if ((off_t)dest_ptr & 7) {
    off_t prologue = 8 - ((off_t)dest_ptr & 7);
    len -= prologue;
    asm volatile("rep movsb\n"
                 : "=S"(src_ptr), "=D"(dest_ptr), "=c"(prologue)
                 : "0"(src_ptr), "1"(dest_ptr), "2"(prologue)
                 : "memory");
  }
  for (off_t i = len / 64; i; --i) {
    asm volatile(
        "movq (%0), %%mm0\n"
        "movq 8(%0), %%mm1\n"
        "movq 16(%0), %%mm2\n"
        "movq 24(%0), %%mm3\n"
        "movq 32(%0), %%mm4\n"
        "movq 40(%0), %%mm5\n"
        "movq 48(%0), %%mm6\n"
        "movq 56(%0), %%mm7\n"
        "movq %%mm0, (%1)\n"
        "movq %%mm1, 8(%1)\n"
        "movq %%mm2, 16(%1)\n"
        "movq %%mm3, 24(%1)\n"
        "movq %%mm4, 32(%1)\n"
        "movq %%mm5, 40(%1)\n"
        "movq %%mm6, 48(%1)\n"
        "movq %%mm7, 56(%1)\n" ::"r"(src_ptr),
        "r"(dest_ptr)
        : "memory");
    src_ptr += 64;
    dest_ptr += 64;
  }
  asm volatile("emms" ::: "memory");
  // Whatever remains we'll have to memcpy.
  len %= 64;
  if (len) memcpy(dest_ptr, src_ptr, len);
  return dest;
}

inline void fast_u32_copy(uint32_t *dest, const uint32_t *src, size_t count) {
  if (count >= 256) {
    mmx_memcpy(dest, src, count * sizeof(count));
    return;
  }
  asm volatile("rep movsl\n"
               : "=S"(src), "=D"(dest), "=c"(count)
               : "S"(src), "D"(dest), "c"(count)
               : "memory");
}


void lumen::screen::flush(const gfx::rect &orig_area) {
  gfx::rect draw_rect = bounds().intersect(orig_area);

	auto src = back_bitmap->pixels();
	auto right = draw_rect.right();
	auto left = draw_rect.left();
	auto bottom = draw_rect.bottom();
	auto w = width();
	auto h = height();

  for (int y = draw_rect.top(); y < bottom; y++) {
		if (y < 0 || y >= h) continue;
    for (int x = left; x < right; x++) {
      if (x < 0 || x >= w) continue;
      buf[x + y * w] = src[x + y * w];
    }
  }
	
	return;
	/*

  gfx::rect area = bounds().intersect(orig_area);

  auto *front_ptr = back_bitmap->scanline(area.y) + area.x;
  auto *back_ptr = &buf[area.y * width()] + area.x;
	auto w = width();

  uint32_t *to_ptr;
  const uint32_t *from_ptr;
	to_ptr = back_ptr;
	from_ptr = front_ptr;

  for (int y = 0; y < area.h; y++) {
    fast_u32_copy(to_ptr, from_ptr, area.w);
    from_ptr += w; // (const uint32_t *)((const uint8_t *)from_ptr + pitch);
    to_ptr += w; // (uint32_t *)((uint8_t *)to_ptr + pitch);
  }
	*/

  // printf("flush drew %zu pixels\n", ndrawn);
}

const gfx::point &lumen::screen::handle_mouse_input(struct mouse_packet &pkt) {
  // clear the old location
	flush(mouse_rect());

  mouse_pos.set_x(mouse_pos.x() + pkt.dx);
  mouse_pos.set_y(mouse_pos.y() + pkt.dy);
  mouse_pos.constrain(bounds());

  // TODO: buttons
	draw_mouse();

  return mouse_pos;
}

void lumen::screen::draw_mouse(void) {
  auto &cur = cursors[cursor];

  // draw the cursor to the framebuffer (NOT THE DOUBLE BUFFER!!!)
  auto draw_rect = mouse_rect();
  for (int y = draw_rect.top(); y < draw_rect.bottom(); y++) {
		if (y < 0 || y >= height()) continue;
    for (int x = draw_rect.left(); x < draw_rect.right(); x++) {
      if (x < 0 || x >= width()) continue;

      uint32_t pix = cur->get_pixel(x - draw_rect.left(), y - draw_rect.top());
			if ((pix & 0xFF000000) >> 24 != 0xFF) {
				uint32_t bg = back_bitmap->get_pixel(x, y);
				pix = blend(pix, bg);
			}

      buf[x + y * width()] = pix;
    }
  }

}
