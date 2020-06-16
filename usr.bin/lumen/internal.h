#pragma once

#include <chariot/fb.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>

namespace lumen {

  // represents a framebuffer for a screen
  class screen {
    int fd = -1;
    size_t bufsz = 0;
    uint32_t *buf = NULL;
    struct ck_fb_info info;

    void flush_info(void) {
      info.active = 1;
      ioctl(fd, FB_SET_INFO, &info);
    }

    void load_info(void) { ioctl(fd, FB_GET_INFO, &info); }

   public:
    screen(int w, int h);
    ~screen(void);

    void set_resolution(int w, int h);
    inline size_t screensize(void) { return bufsz; }
    inline uint32_t *pixels(void) { return buf; }

		inline int width(void) {return info.width;}
		inline int height(void) {return info.height;}
  };


  void set_pixel(int i, int color);
  void set_pixel(int x, int y, int color);
  void set_resolution(int w, int h);

}  // namespace lumen
